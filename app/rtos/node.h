/**
 * @file node.h
 * @brief Application entry point shared by firmware and simulation.
 */
#ifndef SN_NODE_H
#define SN_NODE_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Create all tasks and start the scheduler; does not return unless the scheduler fails. */
void sn_node_run(void);

#ifdef __cplusplus
}
#endif

#endif /* SN_NODE_H */
