# TholusOS Integration Fixtures

These JSON files were captured from the current STK local API using the Syphon
relay build after the Phase 5.3 contract freeze.

Default contract:

```text
http://127.0.0.1:8890/immersive/v1
X-STK-Relay-Token: immersive-dev-token
```

The fixture capture used a temporary port, so `capabilities.api_port` may differ
from the default. `capabilities.default_base_url` is the frozen default.

The requested fixtures are present, plus a few extra captured files useful for
Swift Codable tests:

- `operator_configure_6p_response.json`
- `operator_state_operator_only_results.json`
- `state_action_window_navigator.json`
- `state_final_sprint_roles.json`
- `crew_action_invalid_token.json`

`state_idle.json` is the practical headless local idle/ready state after a boot
normalization reset. The official TholusOS smoke does the same reset before
configure.
