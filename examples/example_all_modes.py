"""
Example: Demonstrating All Modes
"""

import sys
from pathlib import Path
import json

# Add parent to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from meta_ai_system import MetaAISystem, AgentOrchestrator


def print_section(title):
    """Print section header"""
    print("\n" + "=" * 80)
    print(title)
    print("=" * 80)


def demonstrate_meta_mode(orchestrator):
    """Demonstrate Meta Mode"""
    print_section("META MODE - Strategic Controller")

    orchestrator.process_command("Route: Meta")

    result = orchestrator.execute_task({
        "mode": "meta",
        "input_data": {
            "query": "I need to build an AI-powered SaaS product, launch it, and raise funding",
            "context": {"stage": "ideation", "background": "technical founder"},
            "target_domains": ["automation", "gtm", "fundraising"]
        }
    })

    if result["success"]:
        meta_result = result["result"]
        print("\nStrategic Summary:")
        print(meta_result["strategic_summary"][:200] + "...")

        print("\nTactical Playbook (first 3 steps):")
        for i, step in enumerate(meta_result["tactical_playbook"][:3], 1):
            print(f"  {i}. {step}")

        print("\nRelevant Modes Identified:")
        for mode in meta_result["metadata"]["relevant_modes"]:
            print(f"  • {mode}")


def demonstrate_automation_mode(orchestrator):
    """Demonstrate Automation Mode"""
    print_section("AUTOMATION MODE - AI-Automation Architect")

    orchestrator.process_command("Route: Automation")

    result = orchestrator.execute_task({
        "mode": "automation",
        "input_data": {
            "system_type": "multi_agent",
            "requirements": [
                "Multi-agent orchestration",
                "Task queue management",
                "State persistence",
                "Error recovery"
            ],
            "constraints": {"tech_stack": "Python", "budget": "moderate"}
        }
    })

    if result["success"]:
        auto_result = result["result"]

        print("\nSystem Architecture:")
        arch = auto_result["system_architecture"]
        print(f"  Pattern: {arch['pattern']}")
        print(f"  Tech Options: {', '.join(arch['tech_recommendations'][:2])}")

        print("\nImplementation Phases:")
        for phase in auto_result["implementation_phases"][:2]:
            print(f"  • {phase['phase']}: {phase['duration_estimate']}")

        print("\nScalability Patterns (first 3):")
        for pattern in auto_result["scalability_patterns"][:3]:
            print(f"  • {pattern}")


def demonstrate_creative_mode(orchestrator):
    """Demonstrate Creative AI Mode"""
    print_section("CREATIVE AI MODE - Creative Systems Designer")

    orchestrator.process_command("Route: Creative AI")

    result = orchestrator.execute_task({
        "mode": "creative",
        "input_data": {
            "project_type": "audio",
            "style_requirements": {
                "genre": "ambient electronic",
                "mood": "atmospheric",
                "duration": "3-5 minutes"
            },
            "constraints": {"quality": "high", "budget": "moderate"}
        }
    })

    if result["success"]:
        creative_result = result["result"]

        print("\nSignal Chain:")
        chain = creative_result["signal_chain_design"]
        print(f"  Stages: {len(chain['stages'])} processing stages")
        for i, stage in enumerate(chain["stages"][:3], 1):
            print(f"    {i}. {stage}")

        print("\nRecommended Models:")
        for model in chain["models"][:3]:
            print(f"  • {model}")

        print("\nProduction Pipeline:")
        for phase in creative_result["production_pipeline"][:2]:
            print(f"  {phase['phase']}: {', '.join(phase['tasks'][:2])}")


def demonstrate_gtm_mode(orchestrator):
    """Demonstrate GTM Mode"""
    print_section("GTM MODE - Go-to-Market Strategy")

    orchestrator.process_command("Route: GTM")

    result = orchestrator.execute_task({
        "mode": "gtm",
        "input_data": {
            "product": {
                "name": "AI Workflow Automation Platform",
                "description": "No-code AI automation for businesses",
                "stage": "MVP"
            },
            "target_market": {
                "segment": "Small and medium businesses",
                "size": "~10M companies",
                "pain_point": "Time spent on manual repetitive tasks"
            },
            "current_stage": "validation"
        }
    })

    if result["success"]:
        gtm_result = result["result"]

        print("\nPositioning Framework:")
        positioning = gtm_result["positioning_strategy"]
        for key, value in list(positioning["components"].items())[:3]:
            print(f"  {key}: {value}")

        print("\nPricing Models:")
        pricing = gtm_result["pricing_strategy"]
        for model_name, model_info in list(pricing["pricing_models"].items())[:2]:
            print(f"  • {model_name}: {model_info['best_for']}")

        print("\nAcquisition Channels:")
        channels = gtm_result["acquisition_channels"]
        for channel_name in list(channels["channel_categories"].keys())[:3]:
            print(f"  • {channel_name}")


def demonstrate_brand_mode(orchestrator):
    """Demonstrate Brand Mode"""
    print_section("BRAND MODE - Personal Brand Growth")

    orchestrator.process_command("Route: Brand")

    result = orchestrator.execute_task({
        "mode": "brand",
        "input_data": {
            "current_position": {
                "expertise": "AI automation and systems",
                "audience_size": 500,
                "platforms": ["linkedin", "twitter"]
            },
            "goals": [
                "Grow to 10,000 followers",
                "Establish thought leadership in AI",
                "Generate consulting opportunities"
            ],
            "platforms": ["linkedin", "twitter", "newsletter", "youtube"]
        }
    })

    if result["success"]:
        brand_result = result["result"]

        print("\nMessaging Framework:")
        messaging = brand_result["messaging_framework"]
        for key in list(messaging["positioning_framework"].keys())[:3]:
            print(f"  • {key}: {messaging['positioning_framework'][key]}")

        print("\nContent Strategy:")
        content = brand_result["content_strategy"]
        print(f"  Content Types: {len(content['content_types'])} types")
        for content_type, details in list(content["content_types"].items())[:2]:
            print(f"    • {content_type}: {details['purpose']}")

        print("\nGrowth Tactics (first 3):")
        for tactic in brand_result["growth_tactics"]["organic_growth"][:3]:
            print(f"  • {tactic}")


def demonstrate_fundraising_mode(orchestrator):
    """Demonstrate Fundraising Mode"""
    print_section("FUNDRAISING MODE - Investor Strategy")

    orchestrator.process_command("Route: Fundraising")

    result = orchestrator.execute_task({
        "mode": "fundraising",
        "input_data": {
            "company_info": {
                "name": "AI Systems Co",
                "description": "AI automation platform",
                "founded": "2024"
            },
            "round_details": {
                "stage": "seed",
                "target_amount": "$1.5M",
                "use_of_funds": "Product development, team, go-to-market"
            },
            "traction": {
                "users": 500,
                "revenue": "$10K MRR",
                "growth_rate": "20% MoM"
            }
        }
    })

    if result["success"]:
        fundraising_result = result["result"]

        print("\nPitch Structure:")
        pitch = fundraising_result["pitch_structure"]
        for section_name, section_info in list(pitch.items())[:3]:
            if isinstance(section_info, dict):
                print(f"  {section_name}: {section_info.get('duration', 'N/A')}")

        print("\nDeck Architecture (first 5 slides):")
        for slide in fundraising_result["deck_architecture"][:5]:
            print(f"  {slide['slide']}. {slide['title']}")

        print("\nInvestor Types:")
        targeting = fundraising_result["investor_targeting"]
        for investor_type, details in list(targeting["investor_types"].items())[:2]:
            print(f"  • {investor_type}: {details['check_size']}")


def main():
    print("=" * 80)
    print("DEMONSTRATING ALL MODES")
    print("=" * 80)

    # Initialize system
    print("\nInitializing Meta AI System...")
    meta_system = MetaAISystem(workspace_path="C:\\AI Workforce")
    orchestrator = AgentOrchestrator(meta_system)
    print("✓ System initialized")

    # Demonstrate each mode
    demonstrate_meta_mode(orchestrator)
    demonstrate_automation_mode(orchestrator)
    demonstrate_creative_mode(orchestrator)
    demonstrate_gtm_mode(orchestrator)
    demonstrate_brand_mode(orchestrator)
    demonstrate_fundraising_mode(orchestrator)

    # Final status
    print_section("FINAL SYSTEM STATUS")
    status = orchestrator.get_system_status()
    print(f"\nCurrent Mode: {status['current_mode']}")
    print(f"Mode Switches: {status['mode_history_size']}")
    print(f"Available Modes: {len(status['available_modes'])}")

    print("\n" + "=" * 80)
    print("All modes demonstrated successfully!")
    print("=" * 80)


if __name__ == "__main__":
    main()
