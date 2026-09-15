/**
 * @file node.cpp
 * @brief FreeRTOS task wiring: sampling -> filtering -> telemetry, plus command and health tasks.
 *
 * Everything is statically allocated; no heap is used at any time.
 */
#include "node.h"

#include <array>
#include <atomic>
#include <cstdint>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "board.h"
#include "sn/boot_control.h"
#include "sn/bme280.h"
#include "sn/command.hpp"
#include "sn/flash_layout.h"
#include "sn/frame.h"
#include "sn/health.hpp"
#include "sn/processing.hpp"
#include "sn/protocol.hpp"

namespace {

using sn::proto::Telemetry;

constexpr UBaseType_t kPrioSampler = 5U;
constexpr UBaseType_t kPrioFilter = 4U;
constexpr UBaseType_t kPrioCommand = 3U;
constexpr UBaseType_t kPrioTelemetry = 2U;
constexpr UBaseType_t kPrioHealth = 1U;

constexpr std::uint32_t kStackSampler = SN_TASK_STACK_WORDS(384U);
constexpr std::uint32_t kStackFilter = SN_TASK_STACK_WORDS(384U);
constexpr std::uint32_t kStackTelemetry = SN_TASK_STACK_WORDS(384U);
constexpr std::uint32_t kStackCommand = SN_TASK_STACK_WORDS(512U);
constexpr std::uint32_t kStackHealth = SN_TASK_STACK_WORDS(384U);

constexpr std::uint32_t kWatchdogTimeoutMs = 2000U;
constexpr std::uint32_t kCheckinDeadlineMs = 1500U;
constexpr std::uint32_t kConfirmAfterMs = 10000U;
constexpr std::uint32_t kHealthPeriodMs = 250U;
constexpr std::uint32_t kHealthReportMs = 1000U;
constexpr std::uint32_t kQueueWaitMs = 250U;
constexpr std::uint32_t kCommandPollMs = 50U;
constexpr std::uint32_t kSensorRetryMs = 1000U;
constexpr std::uint32_t kResetDrainMs = 50U;
constexpr std::size_t kRawQueueLen = 8U;
constexpr std::size_t kTelemetryQueueLen = 8U;

template <std::uint32_t Words>
struct TaskStorage {
    StaticTask_t tcb;
    std::array<StackType_t, Words> stack;
};

TaskStorage<kStackSampler> g_sampler_mem;
TaskStorage<kStackFilter> g_filter_mem;
TaskStorage<kStackTelemetry> g_telemetry_mem;
TaskStorage<kStackCommand> g_command_mem;
TaskStorage<kStackHealth> g_health_mem;

StaticQueue_t g_raw_queue_mem;
std::array<std::uint8_t, kRawQueueLen * sizeof(sn::RawSample)> g_raw_queue_buf;
StaticQueue_t g_tel_queue_mem;
std::array<std::uint8_t, kTelemetryQueueLen * sizeof(Telemetry)> g_tel_queue_buf;
StaticSemaphore_t g_tx_mutex_mem;

QueueHandle_t g_raw_queue = nullptr;
QueueHandle_t g_tel_queue = nullptr;
SemaphoreHandle_t g_tx_mutex = nullptr;
TaskHandle_t g_tasks[5] = {};
TaskHandle_t g_command_task = nullptr;

sn::HealthMonitor g_health(kCheckinDeadlineMs, kConfirmAfterMs);
std::atomic<std::uint16_t> g_period_ms{sn::proto::kDefaultPeriodMs};
std::atomic<std::uint16_t> g_status{0U};
std::atomic<std::uint32_t> g_sensor_errors{0U};
std::atomic<std::uint32_t> g_frame_errors{0U};
std::atomic<std::uint32_t> g_tx_drops{0U};
std::atomic<std::uint8_t> g_tx_seq{0U};

sn_boot_state_t g_boot_state{};
std::uint8_t g_running_slot = SN_SLOT_A;

std::uint32_t now_ms()
{
    return static_cast<std::uint32_t>(xTaskGetTickCount()) * portTICK_PERIOD_MS;
}

sn_journal_t journal()
{
    return sn_journal_t{board_flash(), SN_JOURNAL_ADDR, SN_JOURNAL_SIZE};
}

std::uint16_t sat16(std::uint32_t v)
{
    return static_cast<std::uint16_t>((v > 0xFFFFU) ? 0xFFFFU : v);
}

/** Serialises frames onto the shared UART. */
void send_payload(const std::uint8_t* payload, std::size_t len)
{
    std::array<std::uint8_t, SN_FRAME_MAX_WIRE> wire{};
    const std::size_t n = sn_frame_encode(payload, len, wire.data(), wire.size());
    if (n == 0U) {
        return;
    }
    (void)xSemaphoreTake(g_tx_mutex, portMAX_DELAY);
    const bool ok = board_uart_write(wire.data(), static_cast<std::uint32_t>(n));
    (void)xSemaphoreGive(g_tx_mutex);
    if (!ok) {
        g_tx_drops.fetch_add(1U, std::memory_order_relaxed);
        g_status.fetch_or(sn::proto::kStatusTxDrops, std::memory_order_relaxed);
    }
}

std::uint8_t next_seq()
{
    return g_tx_seq.fetch_add(1U, std::memory_order_relaxed);
}

sn::proto::Version firmware_version()
{
    const bool trial = (g_boot_state.pending == g_running_slot);
    return sn::proto::Version{SN_VERSION_MAJOR, SN_VERSION_MINOR, SN_VERSION_PATCH, g_running_slot,
                              trial ? g_boot_state.attempts : static_cast<std::uint8_t>(0U),
                              SN_GIT_HASH};
}

void delay_ms(void* ctx, std::uint32_t ms)
{
    (void)ctx;
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/* ---- Sampling: fixed-rate acquisition with drift-free scheduling. ---- */
void sampler_task(void* arg)
{
    (void)arg;
    sn_bme280_bus_t bus = *board_sensor_bus();
    bus.delay_ms = delay_ms;
    sn_bme280_t sensor{};
    bool ready = false;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        if (!ready) {
            ready = (sn_bme280_init(&sensor, &bus) == SN_BME280_OK);
            if (!ready) {
                g_sensor_errors.fetch_add(1U, std::memory_order_relaxed);
                g_status.fetch_or(sn::proto::kStatusSensorFault, std::memory_order_relaxed);
                g_health.checkin(sn::TaskId::kSampler, now_ms());
                vTaskDelay(pdMS_TO_TICKS(kSensorRetryMs));
                last_wake = xTaskGetTickCount();
                continue;
            }
        }
        const TickType_t period = pdMS_TO_TICKS(g_period_ms.load(std::memory_order_relaxed));
        (void)xTaskDelayUntil(&last_wake, period);

        sn::RawSample s{};
        sn_bme280_data_t data{};
        s.timestamp_ms = now_ms();
        s.valid = (sn_bme280_read(&sensor, &data) == SN_BME280_OK);
        if (s.valid) {
            s.temp_cdeg = data.temperature_cdeg;
            s.pressure_pa = data.pressure_pa;
            s.humidity_cpct = data.humidity_cpct;
            g_status.fetch_and(static_cast<std::uint16_t>(~sn::proto::kStatusSensorFault),
                               std::memory_order_relaxed);
        } else {
            g_sensor_errors.fetch_add(1U, std::memory_order_relaxed);
            g_status.fetch_or(sn::proto::kStatusSensorFault, std::memory_order_relaxed);
            ready = false;
        }
        (void)xQueueSend(g_raw_queue, &s, 0U);
        g_health.checkin(sn::TaskId::kSampler, now_ms());
    }
}

/* ---- Filtering and statistics. ---- */
void filter_task(void* arg)
{
    (void)arg;
    static sn::SampleProcessor processor;
    for (;;) {
        sn::RawSample s{};
        if (xQueueReceive(g_raw_queue, &s, pdMS_TO_TICKS(kQueueWaitMs)) == pdPASS) {
            Telemetry t{};
            const std::uint16_t status = g_status.load(std::memory_order_relaxed);
            if (processor.process(s, g_period_ms.load(std::memory_order_relaxed), status, t)) {
                (void)xQueueSend(g_tel_queue, &t, 0U);
            }
        }
        g_health.checkin(sn::TaskId::kFilter, now_ms());
    }
}

/* ---- Telemetry framing. ---- */
void telemetry_task(void* arg)
{
    (void)arg;
    for (;;) {
        Telemetry t{};
        if (xQueueReceive(g_tel_queue, &t, pdMS_TO_TICKS(kQueueWaitMs)) == pdPASS) {
            t.status = static_cast<std::uint16_t>(
                t.status | g_status.fetch_and(static_cast<std::uint16_t>(~sn::proto::kStatusTxDrops),
                                              std::memory_order_relaxed));
            std::array<std::uint8_t, sn::proto::kTelemetrySize> payload{};
            const std::size_t n = sn::proto::encode(t, next_seq(), payload.data(), payload.size());
            send_payload(payload.data(), n);
        }
        g_health.checkin(sn::TaskId::kTelemetry, now_ms());
    }
}

/* ---- Command handling. ---- */
class Control final : public sn::NodeControl {
public:
    void set_period_ms(std::uint16_t period_ms) override { g_period_ms.store(period_ms); }
    void request_reboot() override { reboot_ = true; }
    void request_bootloader() override { bootloader_ = true; }
    sn::proto::Version version() const override { return firmware_version(); }

    void apply_pending() const
    {
        if (reboot_ || bootloader_) {
            vTaskDelay(pdMS_TO_TICKS(kResetDrainMs));
            if (bootloader_) {
                board_enter_bootloader();
            }
            board_reset();
        }
    }

private:
    bool reboot_{false};
    bool bootloader_{false};
};

void on_uart_rx()
{
    if (g_command_task != nullptr) {
        BaseType_t woken = pdFALSE;
        vTaskNotifyGiveFromISR(g_command_task, &woken);
        portYIELD_FROM_ISR(woken);
    }
}

void command_task(void* arg)
{
    (void)arg;
    static sn_frame_decoder_t decoder;
    sn_frame_decoder_init(&decoder);
    Control control;
    std::array<std::uint8_t, 64> rx{};
    std::array<std::uint8_t, SN_FRAME_MAX_PAYLOAD> payload{};
    std::array<std::uint8_t, 32> reply{};

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kCommandPollMs));
        std::uint32_t got = board_uart_read(rx.data(), static_cast<std::uint32_t>(rx.size()));
        while (got > 0U) {
            for (std::uint32_t i = 0U; i < got; ++i) {
                std::size_t len = 0U;
                const sn_frame_status_t st =
                    sn_frame_decoder_push(&decoder, rx[i], payload.data(), payload.size(), &len);
                if (st == SN_FRAME_OK) {
                    const std::size_t n = sn::handle_command(payload.data(), len, control, reply.data(), reply.size());
                    send_payload(reply.data(), n);
                    control.apply_pending();
                } else if (st != SN_FRAME_NONE) {
                    g_frame_errors.fetch_add(1U, std::memory_order_relaxed);
                } else {
                    /* Frame still incomplete. */
                }
            }
            got = board_uart_read(rx.data(), static_cast<std::uint32_t>(rx.size()));
        }
        g_health.checkin(sn::TaskId::kCommand, now_ms());
    }
}

/* ---- Health supervision, watchdog feeding and image confirmation. ---- */
std::uint16_t min_stack_headroom()
{
    UBaseType_t min = ~static_cast<UBaseType_t>(0U);
    for (TaskHandle_t t : g_tasks) {
        if (t != nullptr) {
            const UBaseType_t free_words = uxTaskGetStackHighWaterMark(t);
            min = (free_words < min) ? free_words : min;
        }
    }
    return sat16(static_cast<std::uint32_t>(min));
}

/** CPU load in permille from idle-task run time since the previous sample. */
class CpuLoadMeter {
public:
#if (configGENERATE_RUN_TIME_STATS == 1)
    CpuLoadMeter() : idle_prev_(ulTaskGetIdleRunTimeCounter()), total_prev_(portGET_RUN_TIME_COUNTER_VALUE()) {}

    std::uint16_t sample()
    {
        const std::uint32_t idle = ulTaskGetIdleRunTimeCounter();
        const std::uint32_t total = portGET_RUN_TIME_COUNTER_VALUE();
        const std::uint32_t d_total = total - total_prev_;
        const std::uint32_t d_idle = idle - idle_prev_;
        idle_prev_ = idle;
        total_prev_ = total;
        if ((d_total == 0U) || (d_idle > d_total)) {
            return 0U;
        }
        return sat16(static_cast<std::uint32_t>((static_cast<std::uint64_t>(d_total - d_idle) * 1000U) / d_total));
    }

private:
    std::uint32_t idle_prev_;
    std::uint32_t total_prev_;
#else
    std::uint16_t sample() const { return sn::proto::kCpuLoadUnavailable; }
#endif
};

void send_boot_announcement()
{
    sn::proto::Boot b{firmware_version(), static_cast<sn::proto::ResetCause>(board_reset_cause()),
                      g_boot_state.flags};
    std::array<std::uint8_t, sn::proto::kBootSize> payload{};
    const std::size_t n = sn::proto::encode(b, next_seq(), payload.data(), payload.size());
    send_payload(payload.data(), n);
}

void confirm_image()
{
    bool changed = false;
    const sn_boot_state_t next = sn_boot_confirm(&g_boot_state, g_running_slot, &changed);
    if (changed) {
        const sn_journal_t j = journal();
        if (sn_journal_store(&j, &next)) {
            g_boot_state = next;
            g_status.fetch_and(static_cast<std::uint16_t>(~sn::proto::kStatusTrialBoot), std::memory_order_relaxed);
        }
    }
}

void health_task(void* arg)
{
    (void)arg;
    send_boot_announcement();
    board_watchdog_start(kWatchdogTimeoutMs);

    TickType_t last_wake = xTaskGetTickCount();
    std::uint32_t last_report = now_ms();
    CpuLoadMeter cpu;
    bool led = false;

    for (;;) {
        (void)xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(kHealthPeriodMs));
        const std::uint32_t now = now_ms();
        const sn::HealthVerdict v = g_health.evaluate(now);
        if (v.feed) {
            board_watchdog_feed();
            led = !led;
            board_led(led);
        }
        if (v.confirm) {
            confirm_image();
        }
        if ((now - last_report) >= kHealthReportMs) {
            last_report = now;
            const sn::proto::Health h{now,
                                      cpu.sample(),
                                      min_stack_headroom(),
                                      static_cast<sn::proto::ResetCause>(board_reset_cause()),
                                      g_boot_state.flags,
                                      sat16(board_uart_rx_errors()),
                                      sat16(g_frame_errors.load()),
                                      sat16(g_tx_drops.load()),
                                      sat16(g_sensor_errors.load()),
                                      sat16(g_health.late_cycles())};
            std::array<std::uint8_t, sn::proto::kHealthSize> payload{};
            const std::size_t n = sn::proto::encode(h, next_seq(), payload.data(), payload.size());
            send_payload(payload.data(), n);
        }
    }
}

template <std::uint32_t Words>
TaskHandle_t spawn(TaskFunction_t fn, const char* name, UBaseType_t prio, TaskStorage<Words>& mem)
{
    return xTaskCreateStatic(fn, name, Words, nullptr, prio, mem.stack.data(), &mem.tcb);
}

}  // namespace

extern "C" void sn_node_run(void)
{
    g_running_slot = board_running_slot();
    const sn_journal_t j = journal();
    std::uint32_t seq = 0U;
    (void)sn_journal_load(&j, &g_boot_state, &seq);
    std::uint16_t status = (g_running_slot == SN_SLOT_B) ? static_cast<std::uint16_t>(sn::proto::kStatusSlotB) : 0U;
    if (g_boot_state.pending == g_running_slot) {
        status = static_cast<std::uint16_t>(status | sn::proto::kStatusTrialBoot);
    }
    if ((g_boot_state.flags & SN_BOOT_FLAG_ROLLED_BACK) != 0U) {
        status = static_cast<std::uint16_t>(status | sn::proto::kStatusRolledBack);
    }
    g_status.store(status);

    g_raw_queue = xQueueCreateStatic(kRawQueueLen, sizeof(sn::RawSample), g_raw_queue_buf.data(), &g_raw_queue_mem);
    g_tel_queue = xQueueCreateStatic(kTelemetryQueueLen, sizeof(Telemetry), g_tel_queue_buf.data(), &g_tel_queue_mem);
    g_tx_mutex = xSemaphoreCreateMutexStatic(&g_tx_mutex_mem);

    g_tasks[0] = spawn(sampler_task, "sample", kPrioSampler, g_sampler_mem);
    g_tasks[1] = spawn(filter_task, "filter", kPrioFilter, g_filter_mem);
    g_tasks[2] = spawn(telemetry_task, "telem", kPrioTelemetry, g_telemetry_mem);
    g_tasks[3] = spawn(command_task, "cmd", kPrioCommand, g_command_mem);
    g_tasks[4] = spawn(health_task, "health", kPrioHealth, g_health_mem);
    g_command_task = g_tasks[3];
    board_uart_set_rx_callback(on_uart_rx);

    vTaskStartScheduler();
}

/* ---- Static memory for kernel-owned tasks. ---- */
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** tcb, StackType_t** stack, configSTACK_DEPTH_TYPE* words)
{
    static StaticTask_t idle_tcb;
    static std::array<StackType_t, configMINIMAL_STACK_SIZE> idle_stack;
    *tcb = &idle_tcb;
    *stack = idle_stack.data();
    *words = configMINIMAL_STACK_SIZE;
}

extern "C" void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    (void)task;
    (void)name;
    board_reset();
}
