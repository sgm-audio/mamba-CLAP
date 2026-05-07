"""
Meta Agent - Master controller and orchestrator
"""

from typing import Dict, Any, List
from .base_agent import BaseAgent, ResponseFramework


class MetaAgent(BaseAgent):
    """
    Unified Meta-Mode Agent
    Master controller enabling dynamic switching across all modes
    """

    def __init__(self):
        super().__init__("Meta")

    def process(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        Process meta-level requests with strategic reasoning

        Args:
            input_data: Should contain 'query', 'context', and optional 'target_domains'

        Returns:
            Structured strategic response
        """
        query = input_data.get("query", "")
        context = input_data.get("context", {})
        target_domains = input_data.get("target_domains", [])

        self.logger.info(f"Processing meta query: {query[:100]}")

        # Analyze which modes might be relevant
        relevant_modes = self._identify_relevant_modes(query, target_domains)

        # Generate strategic response
        response = self._generate_strategic_response(query, context, relevant_modes)

        self.log_execution(input_data, response)

        return response

    def _identify_relevant_modes(self, query: str, explicit_domains: List[str]) -> List[str]:
        """Identify which modes are relevant to the query"""

        # Keyword mapping to modes
        mode_keywords = {
            "automation": ["automate", "workflow", "pipeline", "agent", "orchestration", "system architecture"],
            "creative": ["audio", "music", "visual", "creative", "generative", "art", "design system"],
            "gtm": ["monetize", "revenue", "pricing", "customer", "acquisition", "market", "positioning"],
            "brand": ["content", "audience", "platform", "authority", "personal brand", "social"],
            "fundraising": ["investor", "pitch", "fundraising", "deck", "valuation", "capital"]
        }

        relevant = []

        query_lower = query.lower()

        for mode, keywords in mode_keywords.items():
            if any(keyword in query_lower for keyword in keywords):
                relevant.append(mode)

        # Add explicitly requested domains
        relevant.extend(explicit_domains)

        return list(set(relevant))

    def _generate_strategic_response(
        self,
        query: str,
        context: Dict,
        relevant_modes: List[str]
    ) -> Dict[str, Any]:
        """Generate comprehensive strategic response"""

        strategic_summary = self._create_strategic_summary(query, relevant_modes)
        tactical_playbook = self._create_tactical_playbook(query, relevant_modes)
        failure_modes = self._identify_failure_modes(query, relevant_modes)
        acceleration_options = self._create_acceleration_options(relevant_modes)
        enhancements = self._suggest_mode_enhancements(relevant_modes)

        return ResponseFramework.create_response(
            strategic_summary=strategic_summary,
            tactical_playbook=tactical_playbook,
            failure_modes=failure_modes,
            acceleration_options=acceleration_options,
            mode_enhancements=enhancements,
            metadata={
                "relevant_modes": relevant_modes,
                "query": query
            }
        )

    def _create_strategic_summary(self, query: str, relevant_modes: List[str]) -> str:
        """Create high-level strategic summary"""

        summary_parts = [
            f"Strategic Analysis for: {query}\n",
            f"Cross-Domain Synthesis Required: {', '.join(relevant_modes) if relevant_modes else 'General Strategy'}",
            "\nThis request requires founder-level strategic thinking combining:",
        ]

        mode_descriptions = {
            "automation": "system architecture and workflow automation",
            "creative": "creative AI and multimodal production systems",
            "gtm": "go-to-market strategy and monetization",
            "brand": "personal brand development and content strategy",
            "fundraising": "investor relations and capital strategy"
        }

        for mode in relevant_modes:
            if mode in mode_descriptions:
                summary_parts.append(f"  • {mode_descriptions[mode]}")

        return "\n".join(summary_parts)

    def _create_tactical_playbook(self, query: str, relevant_modes: List[str]) -> List[str]:
        """Create actionable tactical steps"""

        playbook = [
            "Conduct comprehensive situation analysis across all relevant domains",
            "Identify key constraints, resources, and leverage points",
            "Design integrated strategy combining technical and business dimensions",
            "Create phased execution plan with clear milestones",
            "Establish feedback loops and optimization mechanisms",
            "Build measurement framework for tracking progress"
        ]

        # Add mode-specific tactics
        if "automation" in relevant_modes:
            playbook.append("Design system architecture and automation workflows")

        if "creative" in relevant_modes:
            playbook.append("Develop creative AI pipeline and production systems")

        if "gtm" in relevant_modes:
            playbook.append("Build go-to-market strategy and monetization model")

        if "brand" in relevant_modes:
            playbook.append("Create content engine and distribution strategy")

        if "fundraising" in relevant_modes:
            playbook.append("Develop investor narrative and fundraising strategy")

        return playbook

    def _identify_failure_modes(self, query: str, relevant_modes: List[str]) -> List[str]:
        """Identify potential failure modes and constraints"""

        failures = [
            "Insufficient resource allocation (time, capital, team)",
            "Lack of clear success metrics and validation criteria",
            "Underestimating technical or market complexity",
            "Poor timing or market readiness",
            "Inadequate feedback loops for course correction"
        ]

        # Add domain-specific failure modes
        if "automation" in relevant_modes:
            failures.append("Over-engineering or premature optimization")

        if "gtm" in relevant_modes:
            failures.append("Misaligned product-market fit or pricing")

        if "fundraising" in relevant_modes:
            failures.append("Weak narrative or insufficient traction metrics")

        return failures

    def _create_acceleration_options(self, relevant_modes: List[str]) -> List[str]:
        """Suggest ways to accelerate execution"""

        options = [
            "Leverage existing frameworks and templates",
            "Parallel workstream execution for independent tasks",
            "Strategic partnership or co-creation opportunities",
            "Focus on highest-leverage activities first",
            "Automate repetitive or low-value tasks"
        ]

        if "automation" in relevant_modes:
            options.append("Use no-code/low-code tools for rapid prototyping")

        if "brand" in relevant_modes:
            options.append("Repurpose content across multiple platforms")

        if "fundraising" in relevant_modes:
            options.append("Build investor pipeline during product development")

        return options

    def _suggest_mode_enhancements(self, relevant_modes: List[str]) -> List[str]:
        """Suggest mode-specific enhancements"""

        enhancements = []

        if "automation" in relevant_modes:
            enhancements.append("Consider switching to Automation mode for detailed system design")

        if "creative" in relevant_modes:
            enhancements.append("Switch to Creative AI mode for production pipeline development")

        if "gtm" in relevant_modes:
            enhancements.append("Switch to GTM mode for monetization strategy")

        if "brand" in relevant_modes:
            enhancements.append("Switch to Brand mode for content and distribution planning")

        if "fundraising" in relevant_modes:
            enhancements.append("Switch to Fundraising mode for pitch and investor strategy")

        return enhancements

    def get_capabilities(self) -> List[str]:
        """Return Meta Agent capabilities"""
        return [
            "Strategic founder-level reasoning",
            "Dynamic mode routing",
            "Cross-domain synthesis",
            "High-level frameworks + tactical depth",
            "Multi-dimensional analysis",
            "Integrated strategy development"
        ]
