"""
Meta AI System - Unified Control Center
A comprehensive system for managing and routing between specialized AI modes
"""

import json
import logging
from datetime import datetime
from typing import Dict, List, Optional, Any
from enum import Enum
from dataclasses import dataclass, asdict
from pathlib import Path


class ModeCategory(Enum):
    """Categories for different operational modes"""
    META = "meta"
    ARCHITECTURE = "architecture"
    CREATIVE = "creative"
    GTM = "gtm"
    BRAND = "brand"
    FUNDRAISING = "fundraising"


class ModeTag(Enum):
    """Tags for mode capabilities"""
    META = "meta"
    ORCHESTRATION = "orchestration"
    TECHNICAL = "technical"
    SYSTEMS = "systems"
    AUDIO = "audio"
    CREATIVE = "creative"
    BUSINESS = "business"
    GROWTH = "growth"
    CONTENT = "content"
    INFLUENCE = "influence"
    CAPITAL = "capital"
    PITCH = "pitch"


@dataclass
class ModeSpecification:
    """Specification for a single operational mode"""
    name: str
    category: ModeCategory
    trigger_command: str
    purpose: str
    tags: List[ModeTag]
    capabilities: List[str]
    response_style: str
    framework: Optional[List[str]] = None

    def to_dict(self) -> Dict:
        """Convert to dictionary for JSON serialization"""
        return {
            "name": self.name,
            "category": self.category.value,
            "trigger_command": self.trigger_command,
            "purpose": self.purpose,
            "tags": [tag.value for tag in self.tags],
            "capabilities": self.capabilities,
            "response_style": self.response_style,
            "framework": self.framework
        }


@dataclass
class TaskContext:
    """Context for a task execution"""
    task_id: str
    mode: str
    input_data: Dict[str, Any]
    timestamp: str
    priority: int = 5
    metadata: Optional[Dict] = None

    def to_dict(self) -> Dict:
        return asdict(self)


class ModeDirectory:
    """Database of all available modes"""

    def __init__(self):
        self.modes: Dict[str, ModeSpecification] = {}
        self._initialize_modes()

    def _initialize_modes(self):
        """Initialize all mode specifications"""

        # Unified Meta-Mode
        self.modes["meta"] = ModeSpecification(
            name="Unified Meta-Mode",
            category=ModeCategory.META,
            trigger_command="Route: Meta",
            purpose="Master controller enabling dynamic switching across all modes",
            tags=[ModeTag.META, ModeTag.ORCHESTRATION],
            capabilities=[
                "Strategic founder-level reasoning",
                "Dynamic mode routing",
                "Cross-domain synthesis",
                "High-level frameworks + tactical depth"
            ],
            response_style="Strategic, comprehensive, multi-dimensional",
            framework=[
                "Strategic Summary",
                "Tactical Playbook",
                "Failure Modes & Constraints",
                "Acceleration Options",
                "Mode-Specific Enhancements"
            ]
        )

        # AI-Automation Architect Mode
        self.modes["automation"] = ModeSpecification(
            name="AI-Automation Architect Mode",
            category=ModeCategory.ARCHITECTURE,
            trigger_command="Route: Automation",
            purpose="Design agent systems, automation pipelines, orchestration flows",
            tags=[ModeTag.TECHNICAL, ModeTag.SYSTEMS],
            capabilities=[
                "System architecture",
                "Workflow orchestration",
                "Error-handling & state management",
                "Scalable automation patterns"
            ],
            response_style="Highly technical, architecture-first, precise"
        )

        # Creative AI Systems Designer Mode
        self.modes["creative"] = ModeSpecification(
            name="Creative AI Systems Designer Mode",
            category=ModeCategory.CREATIVE,
            trigger_command="Route: Creative AI",
            purpose="Build multimodal creative AI pipelines and audio/visual systems",
            tags=[ModeTag.AUDIO, ModeTag.CREATIVE],
            capabilities=[
                "Signal chain design",
                "Generative AI workflows",
                "Audio/visual model stacking",
                "Style control + fidelity optimization"
            ],
            response_style="Creative + technical hybrid with production-level rigor"
        )

        # Founder GTM / Monetization Mode
        self.modes["gtm"] = ModeSpecification(
            name="Founder GTM / Monetization Mode",
            category=ModeCategory.GTM,
            trigger_command="Route: GTM",
            purpose="Optimize monetization, positioning, pricing, and acquisition",
            tags=[ModeTag.BUSINESS, ModeTag.GROWTH],
            capabilities=[
                "Positioning",
                "Offer architecture",
                "Customer segmentation",
                "Growth loops"
            ],
            response_style="Direct, strategic, concise, business-first"
        )

        # Personal Brand Growth Mode
        self.modes["brand"] = ModeSpecification(
            name="Personal Brand Growth Mode",
            category=ModeCategory.BRAND,
            trigger_command="Route: Brand",
            purpose="Build authority, design content engines, grow platforms",
            tags=[ModeTag.CONTENT, ModeTag.INFLUENCE],
            capabilities=[
                "Messaging",
                "Content frameworks",
                "Multi-platform distribution",
                "Authority asset creation"
            ],
            response_style="Messaging-forward, creative, analytically structured"
        )

        # Fundraising / Investor Strategy Mode
        self.modes["fundraising"] = ModeSpecification(
            name="Fundraising / Investor Strategy Mode",
            category=ModeCategory.FUNDRAISING,
            trigger_command="Route: Fundraising",
            purpose="Build decks, refine narratives, target investors",
            tags=[ModeTag.CAPITAL, ModeTag.PITCH],
            capabilities=[
                "Pitch structure",
                "Narrative engineering",
                "Milestone framing",
                "Objection handling"
            ],
            response_style="Persuasive, quantitative, high-level executive tone"
        )

    def get_mode(self, mode_key: str) -> Optional[ModeSpecification]:
        """Retrieve a mode specification by key"""
        return self.modes.get(mode_key.lower())

    def list_modes(self) -> List[str]:
        """List all available modes"""
        return list(self.modes.keys())

    def get_modes_by_category(self, category: ModeCategory) -> List[ModeSpecification]:
        """Get all modes in a specific category"""
        return [mode for mode in self.modes.values() if mode.category == category]

    def get_modes_by_tag(self, tag: ModeTag) -> List[ModeSpecification]:
        """Get all modes with a specific tag"""
        return [mode for mode in self.modes.values() if tag in mode.tags]

    def export_directory(self) -> Dict:
        """Export the entire mode directory"""
        return {
            key: mode.to_dict()
            for key, mode in self.modes.items()
        }


class MetaAISystem:
    """
    Core Meta AI System - Unified control center for all AI Founder Modes
    """

    def __init__(self, workspace_path: str = "C:\\AI Workforce"):
        self.workspace_path = Path(workspace_path)
        self.state_path = self.workspace_path / "meta_ai_system" / "state"
        self.state_path.mkdir(parents=True, exist_ok=True)

        # Initialize logging
        self._setup_logging()

        # Initialize mode directory
        self.mode_directory = ModeDirectory()

        # Current active mode
        self.current_mode: Optional[str] = "meta"
        self.mode_history: List[Dict] = []

        # Task queue for autonomous operation
        self.task_queue: List[TaskContext] = []

        # Load persistent state
        self._load_state()

        self.logger.info("Meta AI System initialized")

    def _setup_logging(self):
        """Configure logging system"""
        log_path = self.workspace_path / "meta_ai_system" / "logs"
        log_path.mkdir(parents=True, exist_ok=True)

        log_file = log_path / f"meta_system_{datetime.now().strftime('%Y%m%d')}.log"

        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            handlers=[
                logging.FileHandler(log_file),
                logging.StreamHandler()
            ]
        )
        self.logger = logging.getLogger("MetaAISystem")

    def _load_state(self):
        """Load persistent state from disk"""
        state_file = self.state_path / "system_state.json"
        if state_file.exists():
            try:
                with open(state_file, 'r') as f:
                    state = json.load(f)
                    self.current_mode = state.get("current_mode", "meta")
                    self.mode_history = state.get("mode_history", [])
                    self.logger.info(f"State loaded: Active mode is {self.current_mode}")
            except Exception as e:
                self.logger.error(f"Failed to load state: {e}")

    def _save_state(self):
        """Save persistent state to disk"""
        state_file = self.state_path / "system_state.json"
        try:
            state = {
                "current_mode": self.current_mode,
                "mode_history": self.mode_history[-100:],  # Keep last 100 entries
                "last_updated": datetime.now().isoformat()
            }
            with open(state_file, 'w') as f:
                json.dump(state, f, indent=2)
            self.logger.info("State saved successfully")
        except Exception as e:
            self.logger.error(f"Failed to save state: {e}")

    def route_to_mode(self, mode_key: str, context: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Route to a specific mode

        Args:
            mode_key: The mode to switch to (meta, automation, creative, gtm, brand, fundraising)
            context: Optional context information for the mode switch

        Returns:
            Dict containing mode information and status
        """
        mode_key = mode_key.lower()
        mode = self.mode_directory.get_mode(mode_key)

        if not mode:
            self.logger.error(f"Invalid mode: {mode_key}")
            return {
                "success": False,
                "error": f"Mode '{mode_key}' not found",
                "available_modes": self.mode_directory.list_modes()
            }

        # Record mode history
        self.mode_history.append({
            "from_mode": self.current_mode,
            "to_mode": mode_key,
            "timestamp": datetime.now().isoformat(),
            "context": context
        })

        # Update current mode
        self.current_mode = mode_key
        self._save_state()

        self.logger.info(f"Switched to mode: {mode.name}")

        return {
            "success": True,
            "mode": mode.to_dict(),
            "message": f"Activated {mode.name}",
            "capabilities": mode.capabilities,
            "response_style": mode.response_style
        }

    def parse_route_command(self, command: str) -> Optional[str]:
        """
        Parse a route command and extract the mode

        Args:
            command: Command string (e.g., "Route: Meta", "Route: Automation")

        Returns:
            Mode key if valid, None otherwise
        """
        if not command.lower().startswith("route:"):
            return None

        mode_part = command[6:].strip().lower()

        # Map command variations to mode keys
        mode_map = {
            "meta": "meta",
            "automation": "automation",
            "creative ai": "creative",
            "creative": "creative",
            "gtm": "gtm",
            "brand": "brand",
            "fundraising": "fundraising"
        }

        return mode_map.get(mode_part)

    def get_current_mode_info(self) -> Dict[str, Any]:
        """Get information about the current active mode"""
        mode = self.mode_directory.get_mode(self.current_mode)
        if mode:
            return {
                "current_mode": self.current_mode,
                "mode_details": mode.to_dict(),
                "mode_history_count": len(self.mode_history)
            }
        return {"error": "Current mode not found"}

    def add_task(self, mode: str, task_data: Dict[str, Any], priority: int = 5) -> str:
        """
        Add a task to the autonomous operation queue

        Args:
            mode: Mode to execute the task in
            task_data: Task data and parameters
            priority: Priority level (1-10, higher is more urgent)

        Returns:
            Task ID
        """
        task_id = f"task_{datetime.now().strftime('%Y%m%d_%H%M%S_%f')}"

        task = TaskContext(
            task_id=task_id,
            mode=mode,
            input_data=task_data,
            timestamp=datetime.now().isoformat(),
            priority=priority
        )

        self.task_queue.append(task)
        self.task_queue.sort(key=lambda x: x.priority, reverse=True)

        self.logger.info(f"Task added: {task_id} (mode: {mode}, priority: {priority})")

        return task_id

    def get_task_queue(self) -> List[Dict]:
        """Get the current task queue"""
        return [task.to_dict() for task in self.task_queue]

    def export_system_state(self) -> Dict[str, Any]:
        """Export complete system state"""
        return {
            "current_mode": self.current_mode,
            "mode_directory": self.mode_directory.export_directory(),
            "mode_history": self.mode_history[-10:],  # Last 10 mode switches
            "task_queue": self.get_task_queue(),
            "system_info": {
                "workspace_path": str(self.workspace_path),
                "timestamp": datetime.now().isoformat()
            }
        }


def main():
    """Initialize and run the Meta AI System"""
    system = MetaAISystem()

    print("=" * 80)
    print("META AI SYSTEM - UNIFIED CONTROL CENTER")
    print("=" * 80)
    print()

    # Display system status
    status = system.get_current_mode_info()
    print(f"Active Mode: {status['current_mode']}")
    print(f"Mode: {status['mode_details']['name']}")
    print(f"Purpose: {status['mode_details']['purpose']}")
    print()

    # Display available modes
    print("Available Modes:")
    print("-" * 80)
    for mode_key in system.mode_directory.list_modes():
        mode = system.mode_directory.get_mode(mode_key)
        print(f"  • {mode.name}")
        print(f"    Command: {mode.trigger_command}")
        print(f"    Purpose: {mode.purpose}")
        print()

    return system


if __name__ == "__main__":
    system = main()
