"""
Meta AI System - Unified Control Center for AI Founder Modes

A comprehensive autonomous AI system for managing specialized AI modes.
"""

__version__ = "1.0.0"
__author__ = "AI Workforce"

from .core.meta_system import MetaAISystem, ModeDirectory
from .orchestrator.agent_orchestrator import AgentOrchestrator
from .orchestrator.autonomous_controller import AutonomousController

__all__ = [
    "MetaAISystem",
    "ModeDirectory",
    "AgentOrchestrator",
    "AutonomousController"
]
