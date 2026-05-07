"""
Main Entry Point for Meta AI System
"""

import sys
import argparse
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from meta_ai_system.core.meta_system import MetaAISystem
from meta_ai_system.orchestrator.agent_orchestrator import AgentOrchestrator
from meta_ai_system.orchestrator.autonomous_controller import AutonomousController
from meta_ai_system.cli.interactive_cli import InteractiveCLI


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Meta AI System - Unified Control Center for AI Founder Modes"
    )

    parser.add_argument(
        "--mode",
        choices=["interactive", "autonomous", "single"],
        default="interactive",
        help="Operation mode"
    )

    parser.add_argument(
        "--command",
        type=str,
        help="Command to execute in single mode"
    )

    parser.add_argument(
        "--workspace",
        type=str,
        default="C:\\AI Workforce",
        help="Workspace path"
    )

    parser.add_argument(
        "--cycle-interval",
        type=int,
        default=60,
        help="Seconds between autonomous cycles"
    )

    parser.add_argument(
        "--max-cycles",
        type=int,
        help="Maximum autonomous cycles to run"
    )

    args = parser.parse_args()

    # Initialize system
    print("Initializing Meta AI System...")
    meta_system = MetaAISystem(workspace_path=args.workspace)
    orchestrator = AgentOrchestrator(meta_system)

    if args.mode == "interactive":
        # Run interactive CLI
        cli = InteractiveCLI(orchestrator)
        cli.run()

    elif args.mode == "autonomous":
        # Run autonomous mode
        controller = AutonomousController(orchestrator, workspace_path=args.workspace)
        print(f"Starting autonomous operation (cycle interval: {args.cycle_interval}s)")

        if args.max_cycles:
            print(f"Will run for {args.max_cycles} cycles")

        controller.run_continuous(
            cycle_interval=args.cycle_interval,
            max_cycles=args.max_cycles
        )

    elif args.mode == "single":
        # Execute single command
        if not args.command:
            print("Error: --command required for single mode")
            sys.exit(1)

        result = orchestrator.process_command(args.command)
        print("\n=== Result ===")
        print(result)

    else:
        parser.print_help()


if __name__ == "__main__":
    main()
