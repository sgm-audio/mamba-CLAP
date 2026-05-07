"""
Base Agent Class - Foundation for all specialized mode agents
"""

from abc import ABC, abstractmethod
from typing import Dict, Any, List, Optional
from datetime import datetime
import logging


class BaseAgent(ABC):
    """Abstract base class for all mode agents"""

    def __init__(self, mode_name: str):
        self.mode_name = mode_name
        self.logger = logging.getLogger(f"Agent.{mode_name}")
        self.execution_history: List[Dict] = []

    @abstractmethod
    def process(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        Process input data and generate response

        Args:
            input_data: Input data for processing

        Returns:
            Dict containing the response and metadata
        """
        pass

    @abstractmethod
    def get_capabilities(self) -> List[str]:
        """Return list of agent capabilities"""
        pass

    def log_execution(self, input_data: Dict, output_data: Dict):
        """Log execution for history tracking"""
        self.execution_history.append({
            "timestamp": datetime.now().isoformat(),
            "input": input_data,
            "output": output_data
        })

    def get_execution_history(self, limit: int = 10) -> List[Dict]:
        """Get recent execution history"""
        return self.execution_history[-limit:]


class ResponseFramework:
    """Standard response framework for structured outputs"""

    @staticmethod
    def create_response(
        strategic_summary: str,
        tactical_playbook: List[str],
        failure_modes: List[str],
        acceleration_options: List[str],
        mode_enhancements: Optional[List[str]] = None,
        metadata: Optional[Dict] = None
    ) -> Dict[str, Any]:
        """Create a structured response following the Meta AI framework"""

        return {
            "strategic_summary": strategic_summary,
            "tactical_playbook": tactical_playbook,
            "failure_modes_and_constraints": failure_modes,
            "acceleration_options": acceleration_options,
            "mode_specific_enhancements": mode_enhancements or [],
            "metadata": metadata or {},
            "timestamp": datetime.now().isoformat()
        }

    @staticmethod
    def format_response_markdown(response: Dict[str, Any]) -> str:
        """Format response as markdown"""

        md = []
        md.append("# Strategic Response\n")

        md.append("## Strategic Summary")
        md.append(response.get("strategic_summary", ""))
        md.append("")

        md.append("## Tactical Playbook")
        for i, item in enumerate(response.get("tactical_playbook", []), 1):
            md.append(f"{i}. {item}")
        md.append("")

        md.append("## Failure Modes & Constraints")
        for item in response.get("failure_modes_and_constraints", []):
            md.append(f"- {item}")
        md.append("")

        md.append("## Acceleration Options")
        for item in response.get("acceleration_options", []):
            md.append(f"- {item}")
        md.append("")

        if response.get("mode_specific_enhancements"):
            md.append("## Mode-Specific Enhancements")
            for item in response["mode_specific_enhancements"]:
                md.append(f"- {item}")
            md.append("")

        return "\n".join(md)
