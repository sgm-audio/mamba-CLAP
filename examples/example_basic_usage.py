"""
Example: Basic Usage of Meta AI System
"""

import sys
from pathlib import Path

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from meta_ai_system import MetaAISystem, AgentOrchestrator


def main():
    print("=" * 80)
    print("Example: Basic Usage of Meta AI System")
    print("=" * 80)

    # Initialize system
    print("\n1. Initializing Meta AI System...")
    meta_system = MetaAISystem(workspace_path="C:\\AI Workforce")
    orchestrator = AgentOrchestrator(meta_system)

    print(f"✓ System initialized in {meta_system.current_mode} mode")

    # Show available modes
    print("\n2. Available Modes:")
    for mode_key in meta_system.mode_directory.list_modes():
        mode = meta_system.mode_directory.get_mode(mode_key)
        print(f"   • {mode.name}")

    # Route to Automation mode
    print("\n3. Routing to Automation Mode...")
    result = orchestrator.process_command("Route: Automation")

    if result["success"]:
        print(f"✓ Successfully routed to {result['mode']['name']}")
        print(f"   Purpose: {result['mode']['purpose']}")

    # Execute a task in Automation mode
    print("\n4. Executing Automation Task...")
    task_result = orchestrator.execute_task({
        "mode": "automation",
        "input_data": {
            "system_type": "multi_agent",
            "requirements": [
                "orchestration",
                "state management",
                "error handling"
            ],
            "constraints": {}
        }
    })

    if task_result["success"]:
        print("✓ Task executed successfully")
        print(f"   Agent: {task_result['agent_capabilities']}")

        # Display architecture recommendation
        arch = task_result["result"]["system_architecture"]
        print(f"\n   Recommended Pattern: {arch['pattern']}")
        print(f"   Components:")
        for comp in arch["components"][:3]:  # First 3 components
            print(f"      - {comp}")

    # Switch to GTM mode
    print("\n5. Routing to GTM Mode...")
    result = orchestrator.process_command("Route: GTM")

    if result["success"]:
        print(f"✓ Successfully routed to {result['mode']['name']}")

    # Execute GTM task
    print("\n6. Executing GTM Task...")
    gtm_result = orchestrator.execute_task({
        "mode": "gtm",
        "input_data": {
            "product": {
                "name": "AI Automation Tool",
                "stage": "MVP"
            },
            "target_market": {
                "segment": "Small businesses"
            },
            "current_stage": "validation"
        }
    })

    if gtm_result["success"]:
        print("✓ GTM strategy generated")

        positioning = gtm_result["result"]["positioning_strategy"]
        print(f"\n   Positioning Framework:")
        for key, value in list(positioning["components"].items())[:3]:
            print(f"      {key}: {value}")

    # Get system status
    print("\n7. System Status:")
    status = orchestrator.get_system_status()
    print(f"   Current Mode: {status['current_mode']}")
    print(f"   Active Agent: {status['current_agent']}")
    print(f"   Task Queue Size: {status['task_queue_size']}")

    print("\n" + "=" * 80)
    print("Example completed successfully!")
    print("=" * 80)


if __name__ == "__main__":
    main()
