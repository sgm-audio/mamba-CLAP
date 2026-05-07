"""
Test Script - Verify Meta AI System Installation and Functionality
"""

import sys
from pathlib import Path

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent))

from meta_ai_system import MetaAISystem, AgentOrchestrator, AutonomousController


def test_system_initialization():
    """Test system initialization"""
    print("Testing system initialization...", end=" ")
    try:
        system = MetaAISystem(workspace_path="C:\\AI Workforce")
        assert system.current_mode == "meta"
        print("[PASSED]")
        return system
    except Exception as e:
        print(f"[FAILED]: {e}")
        return None


def test_mode_directory(system):
    """Test mode directory"""
    print("Testing mode directory...", end=" ")
    try:
        modes = system.mode_directory.list_modes()
        assert len(modes) == 6
        assert "meta" in modes
        assert "automation" in modes
        assert "creative" in modes
        assert "gtm" in modes
        assert "brand" in modes
        assert "fundraising" in modes
        print("[PASSED]")
        return True
    except Exception as e:
        print(f"[FAILED]: {e}")
        return False


def test_orchestrator(system):
    """Test orchestrator"""
    print("Testing orchestrator...", end=" ")
    try:
        orchestrator = AgentOrchestrator(system)
        assert len(orchestrator.agents) == 6
        print("[PASSED]")
        return orchestrator
    except Exception as e:
        print(f"[FAILED]: {e}")
        return None


def test_mode_switching(orchestrator):
    """Test mode switching"""
    print("Testing mode switching...", end=" ")
    try:
        # Switch to automation
        result = orchestrator.process_command("Route: Automation")
        assert result["success"] == True
        assert orchestrator.meta_system.current_mode == "automation"

        # Switch to gtm
        result = orchestrator.process_command("Route: GTM")
        assert result["success"] == True
        assert orchestrator.meta_system.current_mode == "gtm"

        # Switch back to meta
        result = orchestrator.process_command("Route: Meta")
        assert result["success"] == True
        assert orchestrator.meta_system.current_mode == "meta"

        print("[PASSED]")
        return True
    except Exception as e:
        print(f"[FAILED]: {e}")
        return False


def test_agent_execution(orchestrator):
    """Test agent execution"""
    print("Testing agent execution...", end=" ")
    try:
        # Test meta agent
        result = orchestrator.execute_task({
            "mode": "meta",
            "input_data": {
                "query": "test query",
                "context": {},
                "target_domains": []
            }
        })
        assert result["success"] == True
        assert "result" in result

        print("[PASSED]")
        return True
    except Exception as e:
        print(f"[FAILED]: {e}")
        return False


def test_task_queue(system):
    """Test task queue"""
    print("Testing task queue...", end=" ")
    try:
        # Add tasks
        task1 = system.add_task("automation", {"test": "data1"}, priority=5)
        task2 = system.add_task("gtm", {"test": "data2"}, priority=8)

        # Check queue
        queue = system.get_task_queue()
        assert len(queue) == 2

        # Verify priority ordering (higher priority first)
        assert queue[0]["priority"] == 8
        assert queue[1]["priority"] == 5

        # Clear queue for cleanliness
        system.task_queue.clear()

        print("[PASSED]")
        return True
    except Exception as e:
        print(f"[FAILED]: {e}")
        return False


def test_autonomous_controller(orchestrator):
    """Test autonomous controller"""
    print("Testing autonomous controller...", end=" ")
    try:
        controller = AutonomousController(orchestrator, workspace_path="C:\\AI Workforce")

        # Test adding task
        task_id = controller.add_scheduled_task(
            mode="meta",
            task_data={"query": "test"},
            priority=5
        )
        assert task_id is not None

        # Clear queue
        controller.clear_queue()

        print("[PASSED]")
        return True
    except Exception as e:
        print(f"[FAILED]: {e}")
        return False


def test_state_persistence(system):
    """Test state persistence"""
    print("Testing state persistence...", end=" ")
    try:
        # Save state
        system._save_state()

        # Verify state file exists
        state_file = system.state_path / "system_state.json"
        assert state_file.exists()

        print("[PASSED]")
        return True
    except Exception as e:
        print(f"[FAILED]: {e}")
        return False


def test_all_agents(orchestrator):
    """Test all specialized agents"""
    print("Testing all agents...", end=" ")
    try:
        test_data = {
            "automation": {
                "system_type": "multi_agent",
                "requirements": [],
                "constraints": {}
            },
            "creative": {
                "project_type": "audio",
                "style_requirements": {},
                "constraints": {}
            },
            "gtm": {
                "product": {},
                "target_market": {},
                "current_stage": "ideation"
            },
            "brand": {
                "current_position": {},
                "goals": [],
                "platforms": []
            },
            "fundraising": {
                "company_info": {},
                "round_details": {},
                "traction": {}
            }
        }

        for mode, data in test_data.items():
            result = orchestrator.execute_task({
                "mode": mode,
                "input_data": data
            })
            assert result["success"] == True

        print("[PASSED]")
        return True
    except Exception as e:
        print(f"[FAILED]: {e}")
        return False


def main():
    """Run all tests"""
    print("=" * 80)
    print("META AI SYSTEM - FUNCTIONALITY TEST")
    print("=" * 80)
    print()

    tests_passed = 0
    tests_total = 9

    # Run tests
    system = test_system_initialization()
    if system:
        tests_passed += 1

    if system and test_mode_directory(system):
        tests_passed += 1

    orchestrator = test_orchestrator(system) if system else None
    if orchestrator:
        tests_passed += 1

    if orchestrator and test_mode_switching(orchestrator):
        tests_passed += 1

    if orchestrator and test_agent_execution(orchestrator):
        tests_passed += 1

    if system and test_task_queue(system):
        tests_passed += 1

    if orchestrator and test_autonomous_controller(orchestrator):
        tests_passed += 1

    if system and test_state_persistence(system):
        tests_passed += 1

    if orchestrator and test_all_agents(orchestrator):
        tests_passed += 1

    # Summary
    print()
    print("=" * 80)
    print(f"TEST RESULTS: {tests_passed}/{tests_total} tests passed")
    print("=" * 80)

    if tests_passed == tests_total:
        print("\n[SUCCESS] All tests passed! System is fully operational.")
        return 0
    else:
        print(f"\n[ERROR] {tests_total - tests_passed} test(s) failed. Please review errors above.")
        return 1


if __name__ == "__main__":
    exit_code = main()
    sys.exit(exit_code)
