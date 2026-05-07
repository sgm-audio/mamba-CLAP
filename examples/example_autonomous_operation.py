"""
Example: Autonomous Operation of Meta AI System
"""

import sys
from pathlib import Path
import time

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from meta_ai_system import MetaAISystem, AgentOrchestrator, AutonomousController


def main():
    print("=" * 80)
    print("Example: Autonomous Operation")
    print("=" * 80)

    # Initialize system
    print("\n1. Initializing Meta AI System...")
    meta_system = MetaAISystem(workspace_path="C:\\AI Workforce")
    orchestrator = AgentOrchestrator(meta_system)
    controller = AutonomousController(orchestrator, workspace_path="C:\\AI Workforce")

    print("✓ System initialized")

    # Add tasks to queue
    print("\n2. Adding Tasks to Queue...")

    # Task 1: Automation system design
    task1_id = controller.add_scheduled_task(
        mode="automation",
        task_data={
            "system_type": "workflow",
            "requirements": ["n8n integration", "error handling"],
            "constraints": {}
        },
        priority=8
    )
    print(f"✓ Task 1 added: {task1_id} (Automation)")

    # Task 2: GTM strategy
    task2_id = controller.add_scheduled_task(
        mode="gtm",
        task_data={
            "product": {"name": "SaaS Tool", "stage": "launch"},
            "target_market": {"segment": "B2B SMBs"},
            "current_stage": "launch"
        },
        priority=7
    )
    print(f"✓ Task 2 added: {task2_id} (GTM)")

    # Task 3: Brand strategy
    task3_id = controller.add_scheduled_task(
        mode="brand",
        task_data={
            "current_position": {"expertise": "AI", "audience_size": 100},
            "goals": ["grow audience", "thought leadership"],
            "platforms": ["linkedin", "twitter"]
        },
        priority=6
    )
    print(f"✓ Task 3 added: {task3_id} (Brand)")

    # Task 4: Creative AI system
    task4_id = controller.add_scheduled_task(
        mode="creative",
        task_data={
            "project_type": "audio",
            "style_requirements": {"genre": "electronic"},
            "constraints": {}
        },
        priority=5
    )
    print(f"✓ Task 4 added: {task4_id} (Creative)")

    # Show queue
    queue = meta_system.get_task_queue()
    print(f"\n3. Task Queue Status: {len(queue)} tasks pending")

    # Run autonomous cycles
    print("\n4. Starting Autonomous Operation...")
    print("   (Running 3 cycles with 5-second intervals)")

    controller.start()

    for cycle in range(3):
        print(f"\n   --- Cycle {cycle + 1} ---")

        # Run one cycle
        result = controller.run_cycle(max_tasks_per_cycle=2)

        if result["status"] == "completed":
            cycle_info = result["cycle"]
            print(f"   ✓ Processed {cycle_info['tasks_processed']} tasks")
            print(f"   Duration: {cycle_info['duration_seconds']:.2f}s")

            # Show task results
            for task_result in cycle_info["results"]:
                success = task_result["result"].get("success", False)
                status_icon = "✓" if success else "✗"
                print(f"   {status_icon} {task_result['task_id']}")

        # Wait before next cycle
        if cycle < 2:  # Don't wait after last cycle
            time.sleep(5)

    controller.stop()

    # Show final metrics
    print("\n5. Final Metrics:")
    metrics = controller.get_metrics()
    print(f"   Tasks Completed: {metrics['tasks_completed']}")
    print(f"   Tasks Failed: {metrics['tasks_failed']}")
    print(f"   Success Rate: {metrics['success_rate']:.1f}%")
    print(f"   Uptime: {metrics['uptime_seconds']:.1f}s")

    # Show remaining queue
    remaining_queue = meta_system.get_task_queue()
    print(f"\n6. Remaining Queue: {len(remaining_queue)} tasks")

    # Show execution log
    print("\n7. Recent Executions:")
    recent_log = controller.get_execution_log(limit=3)
    for entry in recent_log:
        print(f"   • {entry['timestamp']}")
        print(f"     Tasks: {entry['tasks_processed']}, Duration: {entry['duration_seconds']:.2f}s")

    print("\n" + "=" * 80)
    print("Autonomous operation example completed!")
    print("=" * 80)


if __name__ == "__main__":
    main()
