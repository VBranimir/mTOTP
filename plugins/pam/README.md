# PAM Plugin (Planned)

This directory is reserved for a future **PAM (Pluggable Authentication Modules)** integration for **mTOTP**.

The intent of this plugin is to explore how a manual, human-executable TOTP variant could be integrated into PAM-compatible authentication flows, such as SSH login or local system authentication, while respecting PAM’s modular design and mTOTP’s strict human-execution constraints.

At present no implementation exists.

This directory serves as a placeholder and a signal of intended direction only.

Contributions, design notes, and conceptual proposals are welcome.  
Implementation work will begin once the core mTOTP mechanism and threat model are sufficiently stabilized.

OS-specific considerations (Linux, BSD, etc.) will be documented if and when implementation work starts.
