---
name: smith-audit-patterns
description: Common security vulnerabilities, code smells, and bug patterns for Smith to scan. Use before running an audit.
---

# Smith Audit Patterns

Use this skill to provide Smith agent with known vulnerability and code smell patterns for comprehensive scanning.

## When to Use

- Smith is about to run an audit on an OpenCode session
- Pre-load patterns to improve scan accuracy
- Custom patterns for your codebase

## Security Vulnerability Patterns

### Critical

| Pattern | Description | Languages |
|---------|--------------|-----------|
| `password\s*=\s*["']` | Hardcoded password | All |
| `api[_-]?key\s*=\s*["']` | Hardcoded API key | All |
| `secret\s*=\s*["']` | Hardcoded secret | All |
| `token\s*=\s*["']` | Hardcoded token | All |
| `eval\s*\(` | Code injection | JS/Python |
| `exec\s*\(` | Command injection | Python |
| `os\.system\s*\(` | Command injection | Python |
| `subprocess.*shell=True` | Shell injection | Python |
| `DROP\s+TABLE` | SQL injection risk | SQL |
| `\.innerHTML\s*=` | XSS vulnerability | JS |
| `dangerouslySetInnerHTML` | React XSS | JSX |
| `os\.environ\[` | Env var exposure | Python |
| `fetch.*credentials` | Auth leakage | JS |

### High

| Pattern | Description | Languages |
|---------|--------------|-----------|
| `crypto\.createCipher` | Weak crypto | Node |
| `MD5` | Weak hash | All |
| `SHA1` | Weak hash | All |
| `Math\.random` | Predictable RNG | JS |
| `http://` | Unencrypted transport | All |
| `\.auth` | Auth config exposure | Config |
| `sudo\s+` | Privilege escalation | Shell |
| `chmod\s+777` | Permissions too open | Shell |
| `sqlalchemy.*text\(.*\%` | SQL injection risk | Python |

### Medium

| Pattern | Description | Languages |
|---------|--------------|-----------|
| `console\.log` | Debug left in | JS |
| `print\s*\(debug` | Debug left in | Python |
| `TODO` | Incomplete implementation | All |
| `FIXME` | Known issue | All |
| `HACK` | Workaround code | All |
| `pass` | Empty except block | Python |
| `except:` | Bare except | Python |
| `// TODO` | Incomplete | JS |
| `/\*.*TODO` | TODO in comment | All |

## Code Smell Patterns

### High

| Pattern | Issue | Languages |
|---------|-------|-----------|
| `function.*\{.*function` | Nested callbacks | JS |
| `for.*\{.*for.*\{` | Deep nesting | All |
| `class.*\{.*class.*\{` | Deep inheritance | OOP |
| `copy` | Unbounded copy | Python |
| `while True` without break | Infinite loop risk | All |
| `sleep\s*\)` in loop | DOS risk | All |

### Medium

| Pattern | Issue | Languages |
|---------|-------|-----------|
| `global\s+\w+` | Global state | Python |
| `var\s+\w+` | Non-const declaration | JS |
| `String\s*\+\s*String` | String concatenation | Java |
| `\w+\.append\(.*\.append\(` | Double append | Python |
| `if.*return.*else.*return` | Redundant condition | All |

## Bug Patterns

### Common Logic Errors

```javascript
// Assignment instead of comparison
if (x = 5) { }  // Should be x === 5

// Off-by-one errors
for (i = 0; i <= array.length; i++)

// Null pointer risk
const value = obj.property.nested  // obj could be null

// Race condition
async function() {
  await save();
  await notify();  // What if save fails after?
}
```

```python
# Mutable default argument
def foo(items=[]):  # Should be items=None

# Closure in loop
for i in range(10):
    threads.append(threading.Thread(target=lambda: print(i)))
# All threads will print 9

# Comparison to None
if value == None:  # Should use "is None"
```

## Audit Prompt Template

Use this to structure the audit request:

```
Scan the codebase for:

1. SECURITY VULNERABILITIES (critical first)
   - Hardcoded secrets (password, api_key, token, secret)
   - Injection risks (SQL, command, code)
   - XSS vulnerabilities
   - Weak cryptography
   - Auth/permissions issues

2. CODE SMELLS
   - Complex functions (>50 lines)
   - Deep nesting (>4 levels)
   - Duplicate code
   - Dead code
   - Magic numbers

3. BUGS
   - Null/undefined handling
   - Race conditions
   - Resource leaks
   - Error handling issues

4. UNFINISHED
   - TODO/FIXME/HACK comments
   - Stub functions
   - Placeholder code

For each issue provide:
- File and line number
- Severity: critical/high/medium/low
- Description
- Suggested fix
```

## Custom Patterns

Add your own patterns based on your codebase:

```markdown
## Custom: MyProject Patterns

### API Patterns
- `api/v1/` - versioned endpoints
- `authMiddleware` - auth check required

### Business Logic
- `PriceCalculator` - price logic
- `OrderProcessor` - order flow
```

## Notes

- Run patterns before audit to prime Smith
- Adjust severity based on your risk tolerance
- Update patterns as codebase evolves
- Store in smith-audit-patterns skill for reuse
