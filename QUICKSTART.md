# Meta AI System - Quick Start Guide

Get started with the Meta AI System in 5 minutes.

## 📦 Installation

No installation required - the system is ready to use!

## 🚀 Running the System

### Option 1: Interactive Mode (Recommended for First Time)

```bash
cd "C:\AI Workforce"
python meta_ai_system/main.py
```

This launches an interactive CLI where you can:
- Switch between modes
- Execute commands
- View system status
- Explore capabilities

### Option 2: Run an Example

```bash
# Basic usage example
python meta_ai_system/examples/example_basic_usage.py

# Autonomous operation example
python meta_ai_system/examples/example_autonomous_operation.py

# Demonstration of all modes
python meta_ai_system/examples/example_all_modes.py
```

## 🎯 Your First Commands

Once in interactive mode:

```
> help
Shows all available commands

> modes
Lists all available modes

> Route: Automation
Switches to Automation mode

> status
Shows current system status

> quit
Exits the system
```

## 📝 Simple Usage Example

```python
from meta_ai_system import MetaAISystem, AgentOrchestrator

# Initialize
system = MetaAISystem()
orchestrator = AgentOrchestrator(system)

# Route to a mode
orchestrator.process_command("Route: GTM")

# Execute a task
result = orchestrator.execute_task({
    "mode": "gtm",
    "input_data": {
        "product": {"name": "My Product", "stage": "MVP"},
        "target_market": {"segment": "SMBs"},
        "current_stage": "validation"
    }
})

print(result)
```

## 🔄 Mode Routing

Switch between modes with these commands:

```
Route: Meta           # Strategic controller
Route: Automation     # System architecture
Route: Creative AI    # Creative systems
Route: GTM           # Go-to-market
Route: Brand         # Personal brand
Route: Fundraising   # Investor strategy
```

## 📊 Common Use Cases

### 1. Design an Automation System

```
Route: Automation

[System will prompt for details about your automation needs]
```

### 2. Develop GTM Strategy

```
Route: GTM

[System will help with positioning, pricing, and acquisition]
```

### 3. Build Content Strategy

```
Route: Brand

[System will create messaging and content frameworks]
```

### 4. Prepare for Fundraising

```
Route: Fundraising

[System will help with pitch, deck, and investor targeting]
```

## 🎛 Autonomous Operation

Schedule tasks to run automatically:

```python
from meta_ai_system import AutonomousController

controller = AutonomousController(orchestrator)

# Add tasks
controller.add_scheduled_task(
    mode="gtm",
    task_data={...},
    priority=8
)

# Run autonomous cycles
controller.run_continuous(cycle_interval=60)
```

## 📖 Next Steps

1. **Read the full README**: `meta_ai_system/README.md`
2. **Run examples**: Try all example scripts
3. **Explore agents**: Check `meta_ai_system/agents/` for detailed capabilities
4. **Review logs**: Check `meta_ai_system/logs/` to see system activity

## 🆘 Getting Help

### In Interactive Mode
```
> help
```

### View Capabilities
```
> capabilities
```

### Check System Status
```
> status
```

### View Mode History
```
> history
```

## 💡 Tips

- **Start with Meta Mode** for strategic analysis
- **Switch modes** as needed for specialized work
- **Use the task queue** for batch processing
- **Check logs** if something unexpected happens
- **Review examples** for usage patterns

## 🐛 Troubleshooting

### System won't start
- Ensure Python is installed
- Check workspace path exists
- Verify file permissions

### Commands not working
- Check spelling of mode names
- Use exact trigger commands
- Review help with `help` command

### No results from agent
- Verify input data format
- Check logs for errors
- Ensure mode is active

## 📞 Support

For detailed documentation, see:
- `README.md` - Complete documentation
- `examples/` - Usage examples
- `logs/` - System logs

---

**You're ready to go! Start with:**

```bash
python meta_ai_system/main.py
```

Then type `help` to see all commands.
