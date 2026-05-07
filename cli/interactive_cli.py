"""
Interactive CLI for Meta AI System
"""

import json
from typing import Dict, Any
from ..agents.base_agent import ResponseFramework


class InteractiveCLI:
    """
    Interactive command-line interface for the Meta AI System
    """

    def __init__(self, orchestrator):
        self.orchestrator = orchestrator
        self.running = False

    def run(self):
        """Run the interactive CLI"""
        self.running = True

        self._print_welcome()
        self._print_current_mode()

        while self.running:
            try:
                command = input("\n> ").strip()

                if not command:
                    continue

                if command.lower() in ["exit", "quit", "q"]:
                    self._handle_exit()
                    break

                elif command.lower() in ["help", "h", "?"]:
                    self._print_help()

                elif command.lower() == "status":
                    self._print_status()

                elif command.lower() == "modes":
                    self._print_modes()

                elif command.lower() == "capabilities":
                    self._print_capabilities()

                elif command.lower() == "history":
                    self._print_history()

                elif command.lower() == "queue":
                    self._print_queue()

                elif command.lower().startswith("route:"):
                    self._handle_route_command(command)

                else:
                    self._handle_command(command)

            except KeyboardInterrupt:
                self._handle_exit()
                break

            except Exception as e:
                print(f"\nError: {e}")

    def _print_welcome(self):
        """Print welcome message"""
        print("\n" + "=" * 80)
        print("META AI SYSTEM - UNIFIED CONTROL CENTER")
        print("=" * 80)
        print("\nType 'help' for commands, 'quit' to exit")

    def _print_current_mode(self):
        """Print current active mode"""
        mode_info = self.orchestrator.meta_system.get_current_mode_info()
        mode_details = mode_info.get("mode_details", {})

        print(f"\n📍 Current Mode: {mode_details.get('name', 'Unknown')}")
        print(f"   Purpose: {mode_details.get('purpose', 'N/A')}")
        print(f"   Style: {mode_details.get('response_style', 'N/A')}")

    def _print_help(self):
        """Print help information"""
        print("\n=== Commands ===")
        print("  help              - Show this help message")
        print("  status            - Show system status")
        print("  modes             - List all available modes")
        print("  capabilities      - Show capabilities of all agents")
        print("  history           - Show mode switch history")
        print("  queue             - Show task queue")
        print("\n=== Routing Commands ===")
        print("  Route: Meta       - Switch to Meta mode")
        print("  Route: Automation - Switch to Automation mode")
        print("  Route: Creative AI - Switch to Creative AI mode")
        print("  Route: GTM        - Switch to GTM mode")
        print("  Route: Brand      - Switch to Brand mode")
        print("  Route: Fundraising - Switch to Fundraising mode")
        print("\n=== Other ===")
        print("  [any text]        - Process command in current mode")
        print("  quit, exit, q     - Exit the system")

    def _print_status(self):
        """Print system status"""
        status = self.orchestrator.get_system_status()

        print("\n=== System Status ===")
        print(f"Current Mode: {status['current_mode']}")
        print(f"Current Agent: {status['current_agent']}")
        print(f"Task Queue Size: {status['task_queue_size']}")
        print(f"Mode History Size: {status['mode_history_size']}")
        print(f"Available Modes: {', '.join(status['available_modes'])}")

    def _print_modes(self):
        """Print all available modes"""
        print("\n=== Available Modes ===")

        for mode_key in self.orchestrator.meta_system.mode_directory.list_modes():
            mode = self.orchestrator.meta_system.mode_directory.get_mode(mode_key)
            current = "→" if mode_key == self.orchestrator.meta_system.current_mode else " "

            print(f"\n{current} {mode.name}")
            print(f"  Command: {mode.trigger_command}")
            print(f"  Purpose: {mode.purpose}")
            print(f"  Category: {mode.category.value}")
            print(f"  Tags: {', '.join([tag.value for tag in mode.tags])}")

    def _print_capabilities(self):
        """Print capabilities of all agents"""
        capabilities = self.orchestrator.get_all_capabilities()

        print("\n=== Agent Capabilities ===")

        for mode, caps in capabilities.items():
            print(f"\n{mode.upper()}:")
            for cap in caps:
                print(f"  • {cap}")

    def _print_history(self):
        """Print mode switch history"""
        history = self.orchestrator.meta_system.mode_history[-10:]

        if not history:
            print("\nNo mode switch history yet")
            return

        print("\n=== Mode Switch History (Last 10) ===")

        for entry in history:
            print(f"\n{entry['timestamp']}")
            print(f"  From: {entry['from_mode']} → To: {entry['to_mode']}")
            if entry.get('context'):
                print(f"  Context: {entry['context']}")

    def _print_queue(self):
        """Print task queue"""
        queue = self.orchestrator.meta_system.get_task_queue()

        if not queue:
            print("\nTask queue is empty")
            return

        print(f"\n=== Task Queue ({len(queue)} tasks) ===")

        for task in queue:
            print(f"\n{task['task_id']}")
            print(f"  Mode: {task['mode']}")
            print(f"  Priority: {task['priority']}")
            print(f"  Timestamp: {task['timestamp']}")

    def _handle_route_command(self, command: str):
        """Handle route command"""
        result = self.orchestrator.process_command(command)

        if result.get("success"):
            print(f"\n✓ {result['message']}")
            self._print_current_mode()
        else:
            print(f"\n✗ Error: {result.get('error', 'Unknown error')}")
            print(f"Available modes: {', '.join(result.get('available_modes', []))}")

    def _handle_command(self, command: str):
        """Handle general command"""
        print("\nProcessing...")

        result = self.orchestrator.process_command(command)

        if result.get("success"):
            self._display_result(result)
        else:
            print(f"\n✗ Error: {result.get('error', 'Unknown error')}")

    def _display_result(self, result: Dict[str, Any]):
        """Display command result"""
        print("\n" + "=" * 80)
        print(f"Mode: {result['mode'].upper()}")
        print("=" * 80)

        agent_result = result.get("result", {})

        # Check if it's a structured response
        if "strategic_summary" in agent_result:
            # Use ResponseFramework to format
            formatted = ResponseFramework.format_response_markdown(agent_result)
            print(formatted)
        else:
            # Display as JSON
            print(json.dumps(agent_result, indent=2))

        print("\n" + "=" * 80)

    def _handle_exit(self):
        """Handle exit"""
        print("\n\nShutting down Meta AI System...")
        print("Saving state...")

        self.orchestrator.meta_system._save_state()

        print("✓ System shutdown complete")
        self.running = False


def main():
    """Run interactive CLI standalone"""
    from ..core.meta_system import MetaAISystem
    from ..orchestrator.agent_orchestrator import AgentOrchestrator

    meta_system = MetaAISystem()
    orchestrator = AgentOrchestrator(meta_system)

    cli = InteractiveCLI(orchestrator)
    cli.run()


if __name__ == "__main__":
    main()
