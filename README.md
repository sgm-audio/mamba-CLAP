# Meta AI System - Unified Control Center

A comprehensive autonomous AI system for managing specialized AI founder modes, including routing, orchestration, and autonomous execution.

## 🎯 Overview

The Meta AI System is a unified control center that enables dynamic switching across specialized AI modes optimized for different aspects of building and scaling startups:

- **Meta Mode**: Master controller for strategic reasoning and cross-domain synthesis
- **Automation Mode**: Design agent systems, automation pipelines, and orchestration flows
- **Creative AI Mode**: Build multimodal creative AI pipelines and audio/visual systems
- **GTM Mode**: Optimize monetization, positioning, pricing, and acquisition
- **Brand Mode**: Build authority, design content engines, and grow platforms
- **Fundraising Mode**: Build decks, refine narratives, and target investors

## 🏗 Architecture

```
meta_ai_system/
├── core/
│   └── meta_system.py          # Core system with mode directory and state management
├── agents/
│   ├── base_agent.py           # Abstract base agent class
│   ├── meta_agent.py           # Meta-mode strategic controller
│   ├── automation_agent.py     # Automation architecture agent
│   ├── creative_agent.py       # Creative AI systems agent
│   ├── gtm_agent.py           # GTM and monetization agent
│   ├── brand_agent.py         # Personal brand growth agent
│   └── fundraising_agent.py   # Fundraising strategy agent
├── orchestrator/
│   ├── agent_orchestrator.py  # Manages and coordinates all agents
│   └── autonomous_controller.py # Enables continuous autonomous operation
├── cli/
│   └── interactive_cli.py      # Interactive command-line interface
└── main.py                     # Main entry point
```

## 🚀 Quick Start

### Installation

```bash
# Navigate to the workspace
cd "C:\AI Workforce"

# Run the system
python meta_ai_system/main.py
```

### Basic Usage

```bash
# Interactive mode (default)
python meta_ai_system/main.py --mode interactive

# Execute single command
python meta_ai_system/main.py --mode single --command "Route: Meta"

# Autonomous operation
python meta_ai_system/main.py --mode autonomous --cycle-interval 60
```

## 📖 Usage Guide

### Interactive Mode

Start the interactive CLI:

```bash
python meta_ai_system/main.py
```

#### Available Commands

```
help              - Show help message
status            - Show system status
modes             - List all available modes
capabilities      - Show capabilities of all agents
history           - Show mode switch history
queue             - Show task queue
quit              - Exit system
```

#### Routing Commands

Switch between modes using route commands:

```
Route: Meta
Route: Automation
Route: Creative AI
Route: GTM
Route: Brand
Route: Fundraising
```

### Programmatic Usage

```python
from meta_ai_system.core.meta_system import MetaAISystem
from meta_ai_system.orchestrator.agent_orchestrator import AgentOrchestrator

# Initialize system
meta_system = MetaAISystem(workspace_path="C:\\AI Workforce")
orchestrator = AgentOrchestrator(meta_system)

# Route to a mode
result = orchestrator.process_command("Route: Automation")

# Execute task in current mode
result = orchestrator.execute_task({
    "mode": "automation",
    "input_data": {
        "system_type": "multi_agent",
        "requirements": ["orchestration", "state management"],
        "constraints": {"budget": "low", "timeline": "fast"}
    }
})

print(result)
```

### Autonomous Operation

```python
from meta_ai_system.orchestrator.autonomous_controller import AutonomousController

# Create controller
controller = AutonomousController(orchestrator, workspace_path="C:\\AI Workforce")

# Add tasks to queue
controller.add_scheduled_task(
    mode="gtm",
    task_data={"product": {...}, "target_market": {...}},
    priority=8
)

# Run autonomous cycles
controller.run_continuous(
    cycle_interval=60,  # 60 seconds between cycles
    max_cycles=100      # Stop after 100 cycles
)

# Check metrics
metrics = controller.get_metrics()
print(f"Tasks completed: {metrics['tasks_completed']}")
print(f"Success rate: {metrics['success_rate']}%")
```

## 🎛 Mode Reference

### Meta Mode
**Purpose**: Master controller enabling dynamic switching across all modes

**Capabilities**:
- Strategic founder-level reasoning
- Dynamic mode routing
- Cross-domain synthesis
- High-level frameworks + tactical depth

**Use When**: Need strategic analysis, cross-domain thinking, or unsure which specialized mode to use

### Automation Mode
**Purpose**: Design agent systems, automation pipelines, orchestration flows

**Capabilities**:
- System architecture
- Workflow orchestration
- Error-handling & state management
- Scalable automation patterns

**Use When**: Building multi-agent systems, automation workflows, or technical infrastructure

### Creative AI Mode
**Purpose**: Build multimodal creative AI pipelines and audio/visual systems

**Capabilities**:
- Signal chain design
- Generative AI workflows
- Audio/visual model stacking
- Style control + fidelity optimization

**Use When**: Creating music, audio, visuals, or multimodal creative content

### GTM Mode
**Purpose**: Optimize monetization, positioning, pricing, and acquisition

**Capabilities**:
- Positioning
- Offer architecture
- Customer segmentation
- Growth loops

**Use When**: Developing go-to-market strategy, pricing, or monetization models

### Brand Mode
**Purpose**: Build authority, design content engines, grow platforms

**Capabilities**:
- Messaging
- Content frameworks
- Multi-platform distribution
- Authority asset creation

**Use When**: Building personal brand, content strategy, or social media presence

### Fundraising Mode
**Purpose**: Build decks, refine narratives, target investors

**Capabilities**:
- Pitch structure
- Narrative engineering
- Milestone framing
- Objection handling

**Use When**: Preparing for fundraising, building pitch decks, or targeting investors

## 🔧 Advanced Features

### State Persistence

The system automatically saves state to disk:

```
meta_ai_system/
└── state/
    └── system_state.json    # Current mode, history, queue
```

### Logging

Comprehensive logging to track all operations:

```
meta_ai_system/
└── logs/
    ├── meta_system_YYYYMMDD.log
    └── execution_log_YYYYMMDD_HHMMSS.json
```

### Task Queue

Add tasks for autonomous execution:

```python
# Add high-priority task
task_id = meta_system.add_task(
    mode="brand",
    task_data={
        "current_position": {...},
        "goals": ["grow linkedin", "launch newsletter"],
        "platforms": ["linkedin", "twitter"]
    },
    priority=9  # 1-10, higher = more urgent
)

# View queue
queue = meta_system.get_task_queue()
```

### Metrics & Monitoring

Track system performance:

```python
# Get system metrics
status = controller.get_status()

print(f"Uptime: {status['metrics']['uptime_seconds']}s")
print(f"Tasks completed: {status['metrics']['tasks_completed']}")
print(f"Success rate: {status['metrics']['success_rate']}%")
```

## 📊 Example Workflows

### Example 1: Building an Automation System

```python
# Route to Automation mode
orchestrator.process_command("Route: Automation")

# Request system design
result = orchestrator.execute_task({
    "mode": "automation",
    "input_data": {
        "system_type": "multi_agent",
        "requirements": [
            "orchestration of multiple AI agents",
            "state management",
            "error handling and recovery",
            "scalable to 100+ concurrent tasks"
        ],
        "constraints": {
            "budget": "moderate",
            "tech_stack": "Python preferred"
        }
    }
})

# Result includes:
# - System architecture
# - Workflow orchestration design
# - State management strategy
# - Error handling patterns
# - Implementation phases
# - Technical stack recommendations
```

### Example 2: Developing GTM Strategy

```python
# Route to GTM mode
orchestrator.process_command("Route: GTM")

# Request strategy
result = orchestrator.execute_task({
    "mode": "gtm",
    "input_data": {
        "product": {
            "name": "AI Workflow Builder",
            "description": "No-code AI automation platform",
            "stage": "MVP"
        },
        "target_market": {
            "segment": "Small business owners",
            "size": "10M businesses",
            "pain_point": "Manual repetitive tasks"
        },
        "current_stage": "validation"
    }
})

# Result includes:
# - Positioning strategy
# - Offer architecture
# - Pricing strategy
# - Customer segmentation
# - Acquisition channels
# - Growth loops
# - Execution roadmap
```

### Example 3: Content Strategy for Personal Brand

```python
# Route to Brand mode
orchestrator.process_command("Route: Brand")

# Request content strategy
result = orchestrator.execute_task({
    "mode": "brand",
    "input_data": {
        "current_position": {
            "expertise": "AI automation",
            "audience_size": 500,
            "platforms": ["linkedin", "twitter"]
        },
        "goals": [
            "Grow to 10K followers",
            "Establish thought leadership",
            "Generate consulting leads"
        ],
        "platforms": ["linkedin", "twitter", "newsletter"]
    }
})

# Result includes:
# - Messaging framework
# - Content strategy
# - Content engine design
# - Distribution strategy
# - Authority assets to create
# - Growth tactics
# - Platform-specific strategies
```

## 🛠 Configuration

### Workspace Path

Set custom workspace path:

```python
meta_system = MetaAISystem(workspace_path="/custom/path")
```

### Cycle Interval

Adjust autonomous cycle timing:

```bash
python meta_ai_system/main.py --mode autonomous --cycle-interval 120  # 2 minutes
```

### Max Cycles

Limit autonomous execution:

```bash
python meta_ai_system/main.py --mode autonomous --max-cycles 50
```

## 📝 API Reference

### MetaAISystem

Core system managing modes and state.

**Methods**:
- `route_to_mode(mode_key, context)` - Switch to specified mode
- `parse_route_command(command)` - Parse route command string
- `add_task(mode, task_data, priority)` - Add task to queue
- `get_current_mode_info()` - Get current mode information
- `get_task_queue()` - Get pending tasks
- `export_system_state()` - Export complete state

### AgentOrchestrator

Manages and coordinates all agents.

**Methods**:
- `execute_task(task_context)` - Execute task with appropriate agent
- `process_command(command, context)` - Process command or route
- `get_current_agent()` - Get active agent
- `get_all_capabilities()` - Get capabilities of all agents
- `process_autonomous_tasks(max_tasks)` - Process tasks from queue
- `get_system_status()` - Get comprehensive status

### AutonomousController

Controls autonomous operation.

**Methods**:
- `start()` - Start autonomous operation
- `stop()` - Stop and save logs
- `run_cycle(max_tasks_per_cycle)` - Run single cycle
- `run_continuous(cycle_interval, max_cycles)` - Run continuously
- `add_scheduled_task(mode, task_data, priority)` - Schedule task
- `get_metrics()` - Get performance metrics
- `get_status()` - Get controller status

## 🎓 Best Practices

1. **Start with Meta Mode** - Use Meta mode for strategic analysis before diving into specialized modes

2. **Use Appropriate Modes** - Switch to specialized modes for domain-specific work

3. **Leverage Task Queue** - Schedule tasks for autonomous execution during off-hours

4. **Monitor Metrics** - Track success rates and optimize task priorities

5. **Review Logs** - Check execution logs to understand system behavior

6. **State Management** - System automatically saves state, but manual backups recommended

## 🐛 Troubleshooting

### Issue: Mode switch not working
**Solution**: Check mode name spelling - use exact trigger commands

### Issue: Task queue not processing
**Solution**: Ensure autonomous controller is running and tasks have valid input data

### Issue: Agent errors
**Solution**: Check logs in `meta_ai_system/logs/` for detailed error information

### Issue: State not persisting
**Solution**: Verify write permissions on workspace directory

## 📜 License

Proprietary - All rights reserved

## 🤝 Contributing

This is a private system. For issues or enhancements, contact the system administrator.

## 📞 Support

For questions or support, refer to the system logs or contact the development team.

---

**Version**: 1.0.0
**Last Updated**: 2026-01-13
