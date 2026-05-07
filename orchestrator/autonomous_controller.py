"""
Autonomous Controller - Enables continuous autonomous operation
"""

import time
import logging
from typing import Dict, Any, List, Optional
from datetime import datetime
from pathlib import Path
import json


class AutonomousController:
    """
    Controls autonomous operation of the Meta AI System
    """

    def __init__(self, orchestrator, workspace_path: str = "C:\\AI Workforce"):
        self.orchestrator = orchestrator
        self.workspace_path = Path(workspace_path)
        self.logger = logging.getLogger("AutonomousController")

        self.is_running = False
        self.execution_log: List[Dict] = []

        # Performance metrics
        self.metrics = {
            "tasks_completed": 0,
            "tasks_failed": 0,
            "modes_switched": 0,
            "uptime_start": None,
            "last_execution": None
        }

    def start(self):
        """Start autonomous operation"""
        self.is_running = True
        self.metrics["uptime_start"] = datetime.now().isoformat()
        self.logger.info("Autonomous controller started")

    def stop(self):
        """Stop autonomous operation"""
        self.is_running = False
        self.logger.info("Autonomous controller stopped")
        self._save_execution_log()

    def run_cycle(self, max_tasks_per_cycle: int = 5) -> Dict[str, Any]:
        """
        Run a single autonomous cycle

        Args:
            max_tasks_per_cycle: Maximum tasks to process in this cycle

        Returns:
            Cycle results
        """
        if not self.is_running:
            return {"status": "not_running"}

        cycle_start = datetime.now()
        self.logger.info("Starting autonomous cycle")

        # Process tasks from queue
        results = self.orchestrator.process_autonomous_tasks(max_tasks=max_tasks_per_cycle)

        # Update metrics
        self.metrics["last_execution"] = datetime.now().isoformat()
        self.metrics["tasks_completed"] += len([r for r in results if r.get("result", {}).get("success")])
        self.metrics["tasks_failed"] += len([r for r in results if not r.get("result", {}).get("success")])

        # Log cycle
        cycle_log = {
            "timestamp": cycle_start.isoformat(),
            "tasks_processed": len(results),
            "duration_seconds": (datetime.now() - cycle_start).total_seconds(),
            "results": results
        }

        self.execution_log.append(cycle_log)

        return {
            "status": "completed",
            "cycle": cycle_log,
            "metrics": self.metrics
        }

    def run_continuous(self, cycle_interval: int = 60, max_cycles: Optional[int] = None):
        """
        Run continuously with specified interval

        Args:
            cycle_interval: Seconds between cycles
            max_cycles: Optional maximum number of cycles to run
        """
        self.start()

        cycles_run = 0

        try:
            while self.is_running:
                if max_cycles and cycles_run >= max_cycles:
                    self.logger.info(f"Reached max cycles: {max_cycles}")
                    break

                # Run cycle
                result = self.run_cycle()

                if result.get("status") == "completed":
                    cycles_run += 1
                    self.logger.info(f"Completed cycle {cycles_run}")

                # Wait for next cycle
                time.sleep(cycle_interval)

        except KeyboardInterrupt:
            self.logger.info("Interrupted by user")
        finally:
            self.stop()

    def add_scheduled_task(self, mode: str, task_data: Dict, priority: int = 5) -> str:
        """
        Add a task to the autonomous queue

        Args:
            mode: Mode to execute in
            task_data: Task data
            priority: Task priority (1-10)

        Returns:
            Task ID
        """
        task_id = self.orchestrator.meta_system.add_task(
            mode=mode,
            task_data=task_data,
            priority=priority
        )

        self.logger.info(f"Scheduled task {task_id} in {mode} mode")

        return task_id

    def get_metrics(self) -> Dict[str, Any]:
        """Get performance metrics"""
        return {
            **self.metrics,
            "uptime_seconds": self._calculate_uptime(),
            "success_rate": self._calculate_success_rate(),
            "queue_size": len(self.orchestrator.meta_system.task_queue)
        }

    def _calculate_uptime(self) -> float:
        """Calculate uptime in seconds"""
        if not self.metrics["uptime_start"]:
            return 0

        start = datetime.fromisoformat(self.metrics["uptime_start"])
        return (datetime.now() - start).total_seconds()

    def _calculate_success_rate(self) -> float:
        """Calculate task success rate"""
        total = self.metrics["tasks_completed"] + self.metrics["tasks_failed"]
        if total == 0:
            return 0.0

        return (self.metrics["tasks_completed"] / total) * 100

    def _save_execution_log(self):
        """Save execution log to disk"""
        log_dir = self.workspace_path / "meta_ai_system" / "logs"
        log_dir.mkdir(parents=True, exist_ok=True)

        log_file = log_dir / f"execution_log_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json"

        try:
            with open(log_file, 'w') as f:
                json.dump({
                    "execution_log": self.execution_log,
                    "metrics": self.metrics
                }, f, indent=2)

            self.logger.info(f"Execution log saved to {log_file}")
        except Exception as e:
            self.logger.error(f"Failed to save execution log: {e}")

    def get_execution_log(self, limit: int = 10) -> List[Dict]:
        """Get recent execution log entries"""
        return self.execution_log[-limit:]

    def clear_queue(self):
        """Clear the task queue"""
        queue_size = len(self.orchestrator.meta_system.task_queue)
        self.orchestrator.meta_system.task_queue.clear()
        self.logger.info(f"Cleared {queue_size} tasks from queue")

    def get_status(self) -> Dict[str, Any]:
        """Get comprehensive controller status"""
        return {
            "is_running": self.is_running,
            "metrics": self.get_metrics(),
            "system_status": self.orchestrator.get_system_status(),
            "recent_executions": self.get_execution_log(limit=5)
        }
