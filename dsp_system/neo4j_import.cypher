// neo4j_import_script.cypher
// Maps the orchestration run into Neo4j.

// 1. Create Agents
MERGE (arch:Agent {name: 'Architect', role: 'design'})
MERGE (code:Agent {name: 'CodeGen', role: 'implementation'})
MERGE (test:Agent {name: 'Tester', role: 'validation'})
MERGE (med:Agent {name: 'Mediator', role: 'friction_resolution'})

// 2. Create the Project and Intent
MERGE (proj:Project {name: 'Analog-Hybrid DSP', intent: 'Warmth'})

// 3. Map the Interactions and Friction
MERGE (arch)-[:PROPOSED_DESIGN {checksum: '0x8A2', state: 'Confident'}]->(proj)
MERGE (code)-[:WROTE_CODE {checksum: '0x3B1', state: 'Sterile'}]->(proj)

// 4. Create the Friction Node
CREATE (f:FrictionEvent {
    type: 'Architecture vs Implementation',
    intensity: 0.85,
    timestamp: datetime()
})
MERGE (arch)-[:INVOLVED_IN]->(f)
MERGE (code)-[:INVOLVED_IN]->(f)

// 5. Trigger the Mediator
MERGE (med)-[:RESOLVED_VIA_SPRINT]->(f)
MERGE (test)-[:VALIDATED_OUTCOME]->(med)

// 6. Soul Drift Detection Query (Example for future use)
// MATCH (a:Actor {role: 'dsp_transform'})-[:HAS_STATE]->(s:State)
// WHERE s.last_modified > a.last_checksum_sync
// RETURN a.actor_id, "WARNING: Parameter drift detected."
