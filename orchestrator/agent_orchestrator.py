"""
Agent Orchestrator - Manages and coordinates all specialized agents
"""

import logging
from typing import Dict, Any, Optional
from ..agents.meta_agent import MetaAgent
from ..agents.automation_agent import AutomationAgent
from ..agents.creative_agent import CreativeAgent
from ..agents.gtm_agent import GTMAgent
from ..agents.brand_agent import BrandAgent
from ..agents.fundraising_agent import FundraisingAgent
from ..core.meta_system import MetaAISystem


class AgentOrchestrator:
    """
    Orchestrates all specialized agents and manages execution
    """

    def __init__(self, meta_system: MetaAISystem):
        self.meta_system = meta_system
        self.logger = logging.getLogger("AgentOrchestrator")

        # Initialize all agents
        self.agents = {
            "meta": MetaAgent(),
            "automation": AutomationAgent(),
            "creative": CreativeAgent(),
            "gtm": GTMAgent(),
            "brand": BrandAgent(),
            "fundraising": FundraisingAgent()
        }

        self.logger.info("Agent Orchestrator initialized with all agents")

    def execute_task(self, task_context: Dict[str, Any]) -> Dict[str, Any]:
        """
        Execute a task using the appropriate agent

        Args:
            task_context: Task context including mode, input_data, etc.

        Returns:
            Execution result from the agent
        """
        mode = task_context.get("mode", self.meta_system.current_mode)
        input_data = task_context.get("input_data", {})

        self.logger.info(f"Executing task in mode: {mode}")

        # Get the appropriate agent
        agent = self.agents.get(mode)

        if not agent:
            self.logger.error(f"No agent found for mode: {mode}")
            return {
                "success": False,
                "error": f"Invalid mode: {mode}",
                "available_modes": list(self.agents.keys())
            }

        try:
            # Execute the agent
            result = agent.process(input_data)

            return {
                "success": True,
                "mode": mode,
                "result": result,
                "agent_capabilities": agent.get_capabilities()
            }

        except Exception as e:
            self.logger.error(f"Error executing task in {mode} mode: {e}")
            return {
                "success": False,
                "mode": mode,
                "error": str(e)
            }

    def process_command(self, command: str, context: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Process a command (e.g., "Route: Meta", "Route: Automation")

        Args:
            command: Command string
            context: Optional context data

        Returns:
            Result of command execution
        """
        self.logger.info(f"Processing command: {command}")

        # Check if it's a route command
        mode_key = self.meta_system.parse_route_command(command)

        if mode_key:
            # Route to the specified mode
            result = self.meta_system.route_to_mode(mode_key, context)

            if result.get("success"):
                self.logger.info(f"Successfully routed to {mode_key} mode")

            return result

        else:
            # Not a route command, execute in current mode
            return self.execute_task({
                "mode": self.meta_system.current_mode,
                "input_data": {
                    "query": command,
                    "context": context or {}
                }
            })

    def get_current_agent(self):
        """Get the currently active agent"""
        return self.agents.get(self.meta_system.current_mode)

    def get_all_capabilities(self) -> Dict[str, list]:
        """Get capabilities of all agents"""
        return {
            mode: agent.get_capabilities()
            for mode, agent in self.agents.items()
        }

    def process_autonomous_tasks(self, max_tasks: int = 10) -> list:
        """
        Process tasks from the autonomous task queue

        Args:
            max_tasks: Maximum number of tasks to process

        Returns:
            List of task results
        """
        results = []
        processed = 0

        while self.meta_system.task_queue and processed < max_tasks:
            task = self.meta_system.task_queue.pop(0)

            self.logger.info(f"Processing autonomous task: {task.task_id}")

            # Switch to task's mode
            self.meta_system.route_to_mode(task.mode)

            # Execute task
            result = self.execute_task({
                "mode": task.mode,
                "input_data": task.input_data
            })

            results.append({
                "task_id": task.task_id,
                "result": result,
                "timestamp": task.timestamp
            })

            processed += 1

        self.logger.info(f"Processed {processed} autonomous tasks")

        return results

    def get_system_status(self) -> Dict[str, Any]:
        """Get comprehensive system status"""
        return {
            "current_mode": self.meta_system.current_mode,
            "current_agent": self.get_current_agent().__class__.__name__,
            "available_modes": list(self.agents.keys()),
            "task_queue_size": len(self.meta_system.task_queue),
            "mode_history_size": len(self.meta_system.mode_history),
            "all_capabilities": self.get_all_capabilities()
        }
