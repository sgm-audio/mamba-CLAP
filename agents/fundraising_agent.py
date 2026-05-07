"""
Fundraising Agent - Fundraising / Investor Strategy Mode
"""

from typing import Dict, Any, List
from .base_agent import BaseAgent


class FundraisingAgent(BaseAgent):
    """
    Fundraising / Investor Strategy Agent
    Build decks, refine narratives, target investors
    """

    def __init__(self):
        super().__init__("Fundraising")

    def process(self, input_data: Dict[str, Any]) -> Dict[str, Any]:
        """
        Process fundraising strategy requests

        Args:
            input_data: Should contain 'company_info', 'round_details', 'traction'

        Returns:
            Fundraising strategy and materials
        """
        company_info = input_data.get("company_info", {})
        round_details = input_data.get("round_details", {})
        traction = input_data.get("traction", {})

        self.logger.info(f"Processing fundraising request for {round_details.get('stage', 'unknown')} round")

        response = {
            "pitch_structure": self._design_pitch_structure(company_info, round_details),
            "narrative_framework": self._develop_narrative(company_info, traction),
            "deck_architecture": self._design_deck_architecture(round_details),
            "milestone_framing": self._frame_milestones(traction, round_details),
            "investor_targeting": self._develop_investor_targeting(company_info, round_details),
            "objection_handling": self._prepare_objection_handling(company_info),
            "fundraising_process": self._design_fundraising_process(round_details),
            "execution_timeline": self._create_execution_timeline(round_details)
        }

        self.log_execution(input_data, response)
        return response

    def _design_pitch_structure(self, company_info: Dict, round_details: Dict) -> Dict[str, Any]:
        """Design pitch structure"""

        return {
            "opening": {
                "duration": "30 seconds",
                "components": [
                    "Bold opening statement or question",
                    "Problem size and urgency",
                    "Your solution in one sentence"
                ],
                "example": "What if [transformational outcome]? [Problem] affects [size of market]. We're [solution]."
            },
            "problem": {
                "duration": "1-2 minutes",
                "components": [
                    "Market pain point",
                    "Current alternatives and their failures",
                    "Cost of inaction",
                    "Why now?"
                ],
                "frameworks": [
                    "Before/After comparison",
                    "Customer story illustrating pain",
                    "Market data showing problem scale"
                ]
            },
            "solution": {
                "duration": "2-3 minutes",
                "components": [
                    "Your product/service",
                    "How it works (demo if possible)",
                    "Key differentiators",
                    "Why you'll win"
                ],
                "show_dont_tell": "Demo beats slides every time"
            },
            "market_opportunity": {
                "duration": "1-2 minutes",
                "components": [
                    "TAM/SAM/SOM breakdown",
                    "Market trends and tailwinds",
                    "Go-to-market strategy",
                    "Customer segments"
                ],
                "credibility": "Bottom-up market sizing > top-down"
            },
            "traction": {
                "duration": "2-3 minutes",
                "components": [
                    "Key metrics and growth",
                    "Customer testimonials/logos",
                    "Revenue (if any)",
                    "Momentum indicators"
                ],
                "show_trajectory": "Growth rate matters more than absolute numbers early"
            },
            "team": {
                "duration": "1 minute",
                "components": [
                    "Why you're the right team",
                    "Domain expertise",
                    "Previous successes",
                    "Unique insights"
                ],
                "focus": "What makes you unfairly advantaged to win?"
            },
            "ask": {
                "duration": "1 minute",
                "components": [
                    "Amount raising",
                    "Use of funds",
                    "Key milestones to be achieved",
                    "Next round metrics"
                ],
                "clarity": "Be specific about what money unlocks"
            }
        }

    def _develop_narrative(self, company_info: Dict, traction: Dict) -> Dict[str, Any]:
        """Develop compelling narrative"""

        return {
            "narrative_frameworks": {
                "hero_journey": {
                    "structure": [
                        "Ordinary World: Current broken state",
                        "Call to Adventure: Founding insight",
                        "Tests and Allies: Early challenges overcome",
                        "Transformation: Traction and validation",
                        "Return with Elixir: Future vision"
                    ],
                    "best_for": "Consumer, mission-driven companies"
                },
                "inevitability": {
                    "structure": [
                        "Market forces creating opportunity",
                        "Why solution is inevitable",
                        "Why now is the moment",
                        "Why you're positioned to win",
                        "What world looks like when you succeed"
                    ],
                    "best_for": "Infrastructure, platform plays"
                },
                "category_creation": {
                    "structure": [
                        "Old category limitations",
                        "New category you're creating",
                        "Why new category wins",
                        "Your leadership position",
                        "Category dominance path"
                    ],
                    "best_for": "Novel approaches, paradigm shifts"
                }
            },
            "narrative_elements": {
                "origin_story": "Why you started, founding insight that others missed",
                "unique_insight": "Contrarian belief that you hold and is being validated",
                "vision": "Compelling future state when you succeed",
                "unfair_advantage": "What you have that can't be easily replicated",
                "momentum": "Why things are accelerating"
            },
            "emotional_arc": [
                "Start with empathy for problem",
                "Build tension around current failures",
                "Introduce hope with your solution",
                "Create excitement with traction",
                "Inspire with vision of future"
            ]
        }

    def _design_deck_architecture(self, round_details: Dict) -> List[Dict]:
        """Design pitch deck architecture"""

        return [
            {
                "slide": 1,
                "title": "Cover",
                "content": ["Company name, tagline, logo", "Funding ask and round stage"],
                "tips": ["Make it visual and memorable", "Tagline should be clear value prop"]
            },
            {
                "slide": 2,
                "title": "Problem",
                "content": ["Market pain point", "Current solutions and gaps", "Cost or impact"],
                "tips": ["Use customer quotes", "Show real examples", "Make it tangible"]
            },
            {
                "slide": 3,
                "title": "Solution",
                "content": ["Your product/service", "Key features/benefits", "How it works"],
                "tips": ["Show product screenshots", "Demo video if possible", "Keep it simple"]
            },
            {
                "slide": 4,
                "title": "Why Now",
                "content": ["Market timing", "Technology enablers", "Behavioral shifts"],
                "tips": ["Show trends converging", "Explain urgency", "Competitive window"]
            },
            {
                "slide": 5,
                "title": "Market Size",
                "content": ["TAM/SAM/SOM", "Market trends", "Customer segments"],
                "tips": ["Bottom-up calculation", "Show wedge strategy", "Be credible"]
            },
            {
                "slide": 6,
                "title": "Product",
                "content": ["Product screenshots/demo", "Key workflows", "Differentiators"],
                "tips": ["Show, don't just tell", "Highlight unique features", "User perspective"]
            },
            {
                "slide": 7,
                "title": "Traction",
                "content": ["Key metrics", "Growth charts", "Customer logos/testimonials"],
                "tips": ["Show growth rate", "Hockey stick if you have it", "Cohort analysis"]
            },
            {
                "slide": 8,
                "title": "Business Model",
                "content": ["Revenue model", "Unit economics", "Pricing", "LTV/CAC"],
                "tips": ["Show path to profitability", "Demonstrate scalability"]
            },
            {
                "slide": 9,
                "title": "Competition",
                "content": ["Competitive landscape", "Your differentiation", "Moats"],
                "tips": ["2x2 matrix or table", "Show why you'll win", "Acknowledge competition"]
            },
            {
                "slide": 10,
                "title": "Go-To-Market",
                "content": ["Customer acquisition strategy", "Sales/marketing plan", "Partnerships"],
                "tips": ["Show what's working", "Scalable channels", "Early proof points"]
            },
            {
                "slide": 11,
                "title": "Team",
                "content": ["Founders and key hires", "Relevant experience", "Advisors"],
                "tips": ["Show domain expertise", "Previous wins", "Complementary skills"]
            },
            {
                "slide": 12,
                "title": "Financials",
                "content": ["Revenue projections", "Key assumptions", "Path to profitability"],
                "tips": ["3-5 year forecast", "Show leverage", "Be defensible"]
            },
            {
                "slide": 13,
                "title": "The Ask",
                "content": ["Amount raising", "Use of funds", "Milestones", "Timeline"],
                "tips": ["Be specific", "Show ROI", "Clear next round target"]
            },
            {
                "slide": 14,
                "title": "Vision",
                "content": ["Long-term vision", "Impact", "Why this matters"],
                "tips": ["Inspire", "Show ambition", "Make memorable"]
            }
        ]

    def _frame_milestones(self, traction: Dict, round_details: Dict) -> Dict[str, Any]:
        """Frame milestones and progress"""

        return {
            "milestone_categories": {
                "product": [
                    "MVP launch",
                    "Product-market fit indicators",
                    "Key features shipped",
                    "Platform completeness"
                ],
                "traction": [
                    "User/customer count milestones",
                    "Revenue milestones",
                    "Growth rate achievements",
                    "Retention/engagement metrics"
                ],
                "team": [
                    "Key hires made",
                    "Team size growth",
                    "Advisory board assembled",
                    "Organizational capabilities"
                ],
                "market": [
                    "Market validation",
                    "Partnerships secured",
                    "Awards/recognition",
                    "Media coverage"
                ]
            },
            "milestone_presentation": {
                "past": "What you've accomplished to date (build credibility)",
                "current": "Current focus and near-term targets",
                "future": "What this round will unlock"
            },
            "milestone_framing_tips": [
                "Use specific numbers and dates",
                "Show acceleration and momentum",
                "Connect milestones to round goals",
                "Demonstrate capital efficiency",
                "Compare to industry benchmarks"
            ],
            "next_round_metrics": {
                "pre_seed": "PMF signals, early traction (10-100 users)",
                "seed": "Product-market fit, $100K+ ARR, clear growth",
                "series_a": "$1M+ ARR, strong retention, efficient growth",
                "series_b": "$10M+ ARR, category leadership, international"
            }
        }

    def _develop_investor_targeting(self, company_info: Dict, round_details: Dict) -> Dict[str, Any]:
        """Develop investor targeting strategy"""

        return {
            "investor_segmentation": {
                "thesis_fit": {
                    "description": "VCs with explicit focus on your sector/stage",
                    "priority": "Tier 1 - Highest probability",
                    "research": "Study portfolio, blog posts, Twitter for thesis"
                },
                "portfolio_synergy": {
                    "description": "VCs where you complement existing portfolio",
                    "priority": "Tier 2 - Good probability",
                    "research": "Identify portfolio companies you integrate with"
                },
                "stage_appropriate": {
                    "description": "Funds that invest at your stage and check size",
                    "priority": "Tier 3 - Stage fit",
                    "research": "Verify typical check size matches your needs"
                }
            },
            "investor_types": {
                "angels": {
                    "check_size": "$10K - $250K",
                    "best_for": "Pre-seed, domain expertise, warm intros",
                    "approach": "Personal outreach, shared connections"
                },
                "micro_vcs": {
                    "check_size": "$250K - $1M",
                    "best_for": "Seed stage, hands-on support",
                    "approach": "Warm intro, sector-focused funds"
                },
                "traditional_vcs": {
                    "check_size": "$1M+",
                    "best_for": "Series A+, platform resources, network",
                    "approach": "Partner intro, referral from portfolio"
                },
                "strategic": {
                    "check_size": "Varies",
                    "best_for": "Late-stage, partnership, distribution",
                    "approach": "Business development relationships"
                }
            },
            "targeting_process": [
                "Create target list (50-100 firms/angels)",
                "Tier by fit and probability",
                "Research partners and thesis",
                "Map warm intro paths",
                "Prioritize based on intro quality",
                "Create outreach sequence",
                "Track all interactions in CRM"
            ],
            "warm_intro_strategies": [
                "Leverage founders in their portfolio",
                "Ask advisors and mentors",
                "Connect through ecosystem (accelerators, events)",
                "Build relationship on social before asking",
                "Attend events where they speak"
            ]
        }

    def _prepare_objection_handling(self, company_info: Dict) -> List[Dict]:
        """Prepare for common objections"""

        return [
            {
                "objection": "Market is too small",
                "response_framework": [
                    "Start with beachhead, but show expansion",
                    "Bottom-up market sizing with real data",
                    "Show market growth trends",
                    "Analogies to similar markets"
                ]
            },
            {
                "objection": "Competition is fierce / incumbents will crush you",
                "response_framework": [
                    "Acknowledge competition (shows market validation)",
                    "Explain your wedge and differentiation",
                    "Show why incumbents can't or won't do what you do",
                    "Demonstrate early traction against competition"
                ]
            },
            {
                "objection": "Not enough traction yet",
                "response_framework": [
                    "Show leading indicators and trajectory",
                    "Highlight qualitative signals (customer love, retention)",
                    "Frame traction relative to stage and capital deployed",
                    "Show what next milestone requires"
                ]
            },
            {
                "objection": "Team isn't complete",
                "response_framework": [
                    "Show what founding team has accomplished",
                    "Explain hiring plan and who you're targeting",
                    "Leverage advisors to fill gaps",
                    "Show you've done more with less"
                ]
            },
            {
                "objection": "Unit economics don't work",
                "response_framework": [
                    "Show path to positive economics",
                    "Explain what needs to scale or improve",
                    "Compare to similar companies at your stage",
                    "Show early cohorts improving"
                ]
            },
            {
                "objection": "Valuation is too high",
                "response_framework": [
                    "Anchor to comparable companies",
                    "Show capital efficiency and momentum",
                    "Explain round dynamics and demand",
                    "Focus on what value you're creating for investors"
                ]
            }
        ]

    def _design_fundraising_process(self, round_details: Dict) -> Dict[str, Any]:
        """Design fundraising process"""

        return {
            "process_phases": {
                "preparation": {
                    "duration": "4-6 weeks",
                    "activities": [
                        "Build investor target list",
                        "Create and test pitch deck",
                        "Prepare data room",
                        "Secure warm intros",
                        "Practice pitch 20+ times"
                    ],
                    "output": "Ready to launch outreach"
                },
                "outreach": {
                    "duration": "2-3 weeks",
                    "activities": [
                        "Send pitch decks to warm intros",
                        "Schedule initial meetings",
                        "Track responses in CRM",
                        "Follow up strategically"
                    ],
                    "output": "10-20 first meetings booked"
                },
                "meetings": {
                    "duration": "4-6 weeks",
                    "activities": [
                        "First meetings with partners",
                        "Follow-up meetings with teams",
                        "Partner presentations",
                        "Reference checks",
                        "Ongoing updates and momentum"
                    ],
                    "output": "3-5 term sheets"
                },
                "diligence": {
                    "duration": "2-4 weeks",
                    "activities": [
                        "Technical diligence",
                        "Financial diligence",
                        "Market diligence",
                        "Reference checks",
                        "Negotiate terms"
                    ],
                    "output": "Signed term sheet"
                },
                "closing": {
                    "duration": "4-6 weeks",
                    "activities": [
                        "Legal documentation",
                        "Finalize cap table",
                        "Wire funds",
                        "Announce round"
                    ],
                    "output": "Money in bank"
                }
            },
            "fundraising_best_practices": [
                "Create FOMO with multiple conversations simultaneously",
                "Never take first term sheet immediately - create options",
                "Keep existing investors updated and warmed up",
                "Maintain momentum - slow = dead",
                "Continue running business, don't let it stall",
                "Be transparent about process and timeline",
                "Choose investor based on value-add, not just valuation"
            ],
            "communication_strategy": {
                "regular_updates": "Monthly emails to investors showing progress",
                "meeting_follow_up": "Send recap and next steps within 24 hours",
                "create_urgency": "Share other investor interest (tactfully)",
                "be_responsive": "Reply to investor questions within a few hours"
            }
        }

    def _create_execution_timeline(self, round_details: Dict) -> List[Dict]:
        """Create fundraising execution timeline"""

        return [
            {
                "week": "1-2",
                "phase": "Preparation",
                "activities": [
                    "Finalize deck and materials",
                    "Build investor target list",
                    "Secure warm intros",
                    "Practice pitch"
                ],
                "milestone": "Ready to launch"
            },
            {
                "week": "3-4",
                "phase": "Initial Outreach",
                "activities": [
                    "Send decks to 20-30 investors",
                    "Schedule first meetings",
                    "Continue warm intro gathering"
                ],
                "milestone": "10+ meetings scheduled"
            },
            {
                "week": "5-8",
                "phase": "First Meetings",
                "activities": [
                    "Partner meetings",
                    "Refine pitch based on feedback",
                    "Continue outreach to new investors",
                    "Track interest levels"
                ],
                "milestone": "5+ interested investors"
            },
            {
                "week": "9-12",
                "phase": "Follow-ups & Diligence",
                "activities": [
                    "Second meetings with top prospects",
                    "Partner presentations",
                    "Provide due diligence materials",
                    "Negotiate terms"
                ],
                "milestone": "2-3 term sheets"
            },
            {
                "week": "13-16",
                "phase": "Closing",
                "activities": [
                    "Select lead investor",
                    "Finalize terms",
                    "Complete legal docs",
                    "Close round"
                ],
                "milestone": "Money in bank"
            }
        ]

    def get_capabilities(self) -> List[str]:
        """Return Fundraising Agent capabilities"""
        return [
            "Pitch structure",
            "Narrative engineering",
            "Milestone framing",
            "Objection handling",
            "Investor targeting",
            "Deck architecture",
            "Process management"
        ]
