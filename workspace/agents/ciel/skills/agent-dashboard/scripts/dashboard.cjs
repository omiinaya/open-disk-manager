#!/usr/bin/env node
// Agent Dashboard Generator
// Provides a visual overview of agents, skills, sessions, cron, and heartbeats

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const WORKSPACE = '/root/.openclaw/workspace';
const CONFIG_PATH = '/root/.openclaw/openclaw.json';
const AGENTS_DIR = path.join(WORKSPACE, 'agents');
const SKILLS_DIR = path.join(WORKSPACE, 'skills');

function loadConfig() {
  if (!fs.existsSync(CONFIG_PATH)) {
    throw new Error('OpenClaw config not found at ' + CONFIG_PATH);
  }
  return JSON.parse(fs.readFileSync(CONFIG_PATH, 'utf8'));
}

function getAgentIds() {
  if (!fs.existsSync(AGENTS_DIR)) return [];
  return fs.readdirSync(AGENTS_DIR).filter(f => {
    const dir = path.join(AGENTS_DIR, f);
    return fs.statSync(dir).isDirectory();
  });
}

function getSkillName(skillDir) {
  const skillMdPath = path.join(skillDir, 'SKILL.md');
  if (!fs.existsSync(skillMdPath)) return path.basename(skillDir);
  const content = fs.readFileSync(skillMdPath, 'utf8');
  const match = content.match(/^name:\s*(.+)$/m);
  return match ? match[1].trim() : path.basename(skillDir);
}

function listAgentSkills(agentId) {
  const agentSkillDir = path.join(AGENTS_DIR, agentId, 'skills');
  if (!fs.existsSync(agentSkillDir)) return [];
  const dirs = fs.readdirSync(agentSkillDir).filter(f => {
    const dir = path.join(agentSkillDir, f);
    return fs.statSync(dir).isDirectory();
  });
  return dirs.map(skillDirName => {
    const skillDir = path.join(agentSkillDir, skillDirName);
    return {
      name: getSkillName(skillDir),
      path: skillDir
    };
  });
}

function listWorkspaceSkills() {
  if (!fs.existsSync(SKILLS_DIR)) return [];
  const dirs = fs.readdirSync(SKILLS_DIR).filter(f => {
    const dir = path.join(SKILLS_DIR, f);
    return fs.statSync(dir).isDirectory();
  });
  return dirs.map(skillDirName => {
    const skillDir = path.join(SKILLS_DIR, skillDirName);
    return {
      name: getSkillName(skillDir),
      path: skillDir,
      location: 'workspace'
    };
  });
}

function getActiveSessions() {
  try {
    // Use openclaw CLI to list sessions
    const output = execSync('openclaw sessions list --json', { encoding: 'utf-8' });
    const parsed = JSON.parse(output);
    // Ensure it's an array
    return Array.isArray(parsed) ? parsed : (parsed.sessions || []);
  } catch (err) {
    // Fallback: return empty if CLI not available or fails
    return [];
  }
}

function getCronJobs() {
  const config = loadConfig();
  const cronConfig = config.cron || {};
  return {
    enabled: cronConfig.enabled,
    jobs: cronConfig.jobs || []
  };
}

function getAgentHeartbeat(agentId) {
  const hbPath = path.join(AGENTS_DIR, agentId, 'HEARTBEAT.md');
  if (!fs.existsSync(hbPath)) return { exists: false };
  const content = fs.readFileSync(hbPath, 'utf8').trim();
  return {
    exists: true,
    hasTasks: content.length > 50,
    contentPreview: content.substring(0, 200) + (content.length > 200 ? '...' : '')
  };
}

function generateDashboard() {
  const config = loadConfig();
  const agentsConfig = config.agents || {};
  const agentList = agentsConfig.list || [];
  const defaultAgentId = agentList.find(a => a.default)?.id || null;

  const agentIds = getAgentIds();
  const workspaceSkills = listWorkspaceSkills();

  // Build agent data structure
  const agents = agentList.map(agent => {
    const skills = listAgentSkills(agent.id);
    const heartbeat = getAgentHeartbeat(agent.id);
    return {
      id: agent.id,
      name: agent.id,
      default: agent.default || false,
      model: agent.model?.primary || 'default',
      workspace: agent.workspace,
      agentDir: agent.agentDir,
      subagents: agent.subagents?.allowAgents || [],
      skills,
      heartbeat
    };
  });

  // Determine skill ownership conflicts
  const skillOwnership = {};
  for (const agent of agents) {
    for (const skill of agent.skills) {
      const key = skill.name.toLowerCase();
      if (!skillOwnership[key]) {
        skillOwnership[key] = [];
      }
      skillOwnership[key].push(agent.id);
    }
  }
  // Also include workspace skills
  for (const wsSkill of workspaceSkills) {
    const key = wsSkill.name.toLowerCase();
    if (!skillOwnership[key]) {
      skillOwnership[key] = [];
    }
    skillOwnership[key].push('workspace');
  }

  // Find shared skills (multiple owners)
  const sharedSkills = Object.entries(skillOwnership).filter(([_, owners]) => owners.length > 1);

  // Build hierarchy: who can delegate to whom?
  const delegationEdges = [];
  for (const agent of agents) {
    for (const sub of agent.subagents) {
      delegationEdges.push({ from: agent.id, to: sub });
    }
  }

  // Get active sessions (approximate: only if we can query)
  let activeSessions = [];
  try {
    activeSessions = getActiveSessions();
  } catch (e) {}

  // Extract agent activities from sessions
  const agentActivities = {};
  for (const session of activeSessions) {
    if (!session) continue;
    const agentId = session.agentId || session.agent || 'unknown';
    if (!agentActivities[agentId]) {
      agentActivities[agentId] = [];
    }
    agentActivities[agentId].push({
      sessionKey: session.sessionKey || 'n/a',
      channel: session.channel || 'n/a',
      status: session.status || 'n/a',
      lastActivity: session.lastActivity || 'n/a'
    });
  }

  // Get cron jobs
  const cronInfo = getCronJobs();

  // Generate markdown report
  let md = `# Agent Dashboard\n\n`;
  md += `Generated: ${new Date().toISOString()}\n\n`;

  // --- Agent Overview ---
  md += `## Agent Overview\n\n`;
  md += `| Agent | Default | Model | Subagents | Skills | Heartbeat |\n`;
  md += `|-------|---------|-------|-----------|--------|-----------|\n`;
  for (const agent of agents) {
    const hasHb = agent.heartbeat.exists ? '✅' : '❌';
    md += `| **${agent.id}** | ${agent.default ? '⭐' : ''} | \`${agent.model}\` | ${agent.subagents.length} | ${agent.skills.length} | ${hasHb} |\n`;
  }
  md += `\n`;

  // --- Delegation Hierarchy ---
  md += `## Delegation Hierarchy\n\n`;
  if (delegationEdges.length === 0) {
    md += `No delegation configured. All agents operate independently.\n\n`;
  } else {
    md += `\`\`\`mermaid\ngraph TD\n`;
    for (const edge of delegationEdges) {
      md += `  ${edge.from} --> ${edge.to}\n`;
    }
    md += `\`\`\`\n\n`;
  }

  // --- Skill Ownership ---
  md += `## Skill Ownership\n\n`;
  md += `| Skill | Owner(s) |\n`;
  md += `|-------|----------|\n`;
  const allSkillNames = new Set();
  for (const agent of agents) {
    for (const skill of agent.skills) {
      allSkillNames.add(skill.name);
    }
  }
  for (const ws of workspaceSkills) {
    allSkillNames.add(ws.name);
  }
  for (const skillName of [...allSkillNames].sort()) {
    const owners = skillOwnership[skillName.toLowerCase()] || [];
    const ownerStr = owners.join(', ');
    md += `| ${skillName} | ${ownerStr} |\n`;
  }
  if (sharedSkills.length > 0) {
    md += `\n**⚠️ Shared Skills** (multiple owners): ${sharedSkills.map(([name, owners]) => `${name} (${owners.join(', ')})`).join(', ')}\n`;
  }
  md += `\n`;

  // --- Active Sessions ---
  md += `## Active Sessions\n\n`;
  if (activeSessions.length === 0) {
    md += `No active sessions found (or unable to query).\n\n`;
  } else {
    md += `| Agent | Session Key | Channel | Status | Last Activity |\n`;
    md += `|-------|-------------|---------|--------|---------------|\n`;
    for (const act of activeSessions) {
      if (!act) continue;
      const agentId = act.agentId || act.agent || 'unknown';
      const sessionKey = act.sessionKey ? act.sessionKey.substring(0, 12) + '...' : 'n/a';
      md += `| ${agentId} | \`${sessionKey}\` | ${act.channel || 'n/a'} | ${act.status || 'n/a'} | ${act.lastActivity || 'n/a'} |\n`;
    }
    md += `\n`;
  }

  // --- Per-Agent Activity Detail ---
  if (Object.keys(agentActivities).length > 0) {
    md += `## Agent Activities Detail\n\n`;
    for (const [agentId, sessions] of Object.entries(agentActivities)) {
      md += `### ${agentId}\n\n`;
      md += `- Active sessions: ${sessions.length}\n`;
      for (const s of sessions) {
        md += `  - \`${s.sessionKey.substring(0, 12)}...\` on ${s.channel} (${s.status})\n`;
      }
      md += `\n`;
    }
  }

  // --- Cron Jobs ---
  md += `## Cron Jobs\n\n`;
  md += `**Enabled:** ${cronInfo.enabled ? '✅ Yes' : '❌ No'}\n\n`;
  if (cronInfo.jobs && cronInfo.jobs.length > 0) {
    md += `| Job ID | Schedule | Target | Payload |\n`;
    md += `|--------|----------|--------|---------|\n`;
    for (const job of cronInfo.jobs) {
      const schedule = job.schedule || {};
      const payload = job.payload || {};
      md += `| ${job.id || 'n/a'} | \`${schedule.kind} ${JSON.stringify(schedule).substring(0, 40)}...\` | \`${job.sessionTarget}\` | ${payload.kind} |\n`;
    }
    md += `\n`;
  } else {
    md += `No cron jobs configured.\n\n`;
  }

  // --- Heartbeat Configuration ---
  md += `## Heartbeat Configuration\n\n`;
  md += `| Agent | Has Heartbeat | Tasks/Notes |\n`;
  md += `|-------|---------------|-------------|\n`;
  for (const agent of agents) {
    const hb = agent.heartbeat;
    const hasHb = hb.exists ? '✅' : '❌';
    const notes = hb.hasTasks ? 'Has tasks' : 'Empty/default';
    md += `| ${agent.id} | ${hasHb} | ${notes} |\n`;
  }
  md += `\n`;

  // --- Skill Distribution Summary ---
  md += `## Skill Distribution Summary\n\n`;
  const skillCounts = {};
  for (const agent of agents) {
    skillCounts[agent.id] = agent.skills.length;
  }
  skillCounts['workspace'] = workspaceSkills.length;
  md += `| Location | # Skills |\n`;
  md += `|----------|----------|\n`;
  for (const [loc, count] of Object.entries(skillCounts)) {
    md += `| ${loc} | ${count} |\n`;
  }
  md += `\n`;

  // --- Recommendations ---
  md += `## Recommendations\n\n`;
  const recommendations = [];
  if (sharedSkills.length > 0) {
    recommendations.push(`- Resolve shared skill ownership: ${sharedSkills.map(([n, o]) => n).join(', ')}`);
  }
  if (agents.some(a => !a.heartbeat.exists)) {
    const missingHb = agents.filter(a => !a.heartbeat.exists).map(a => a.id).join(', ');
    recommendations.push(`- Consider adding HEARTBEAT.md to agents: ${missingHb}`);
  }
  if (Object.keys(agentActivities).length === 0 && activeSessions.length === 0) {
    recommendations.push(`- No active sessions detected; agents may be idle.`);
  }
  if (cronInfo.jobs.length === 0) {
    recommendations.push(`- No cron jobs configured; consider scheduling periodic tasks.`);
  }
  if (recommendations.length === 0) {
    md += `✅ All agents have unique skill ownership, heartbeat configured, and there are active sessions.\n`;
  } else {
    for (const rec of recommendations) {
      md += rec + '\n';
    }
  }

  return md;
}

// Run and output
try {
  const dashboard = generateDashboard();
  console.log(dashboard);
} catch (err) {
  console.error('Error generating dashboard:', err.message);
  process.exit(1);
}