# Part 1: Application Description - Macless-Haystack

## 1. Application Functionality

Macless-Haystack is a distributed location tracking system enabling users to track Bluetooth Low Energy (BLE) devices using Apple's Find My network without requiring a Mac computer [1]. The application crowdsources location data from Apple devices worldwide for custom tracking devices, similar to Apple AirTags but using open-source firmware and software.

Core functionality includes: (1) **Key Generation** - SECP224R1 elliptic curve keypairs for end-to-end encrypted tracking [2]; (2) **Device Broadcasting** - ESP32/NRF5x devices broadcast advertisement keys detected by Apple devices [3]; (3) **Location Collection** - Apple devices encrypt and upload location data to iCloud; (4) **Authentication & Retrieval** - backend endpoint retrieves encrypted reports using Apple ID credentials; (5) **Decryption & Visualization** - Flutter frontend decrypts reports and displays locations on interactive maps.

The system operates independently of Apple's official hardware, accessible on Linux, Windows, and Android while maintaining Find My network's privacy and security features.

## 2. Stakeholders

### Primary Users (Device Owners)
Individuals tracking personal items with custom BLE devices. Require technical knowledge but benefit from lower costs [4].

### System Administrators (Endpoint Operators)
Host backend servers, manage Apple ID accounts, ensure availability. Many self-host for privacy.

### Apple Inc. (Infrastructure Provider)
Provides Find My network, iCloud authentication, report storage. Role is involuntary but foundational.

### Apple Device Owners (Crowdsourced Reporters)
Millions of iPhone/iPad/Mac users passively detecting and reporting BLE advertisements [5].

### Anisette Server Operators
Provide device provisioning metadata for non-Apple device authentication [6].

### Open-Source Contributors
Develop firmware, backends, frontends for Macless-Haystack and dependencies.

## 3. Data Collection and Purpose

### 3.1 Cryptographic Keys
SECP224R1 keypairs (28 bytes) generated locally. Public keys enable BLE broadcasting; private keys decrypt reports [7].

### 3.2 BLE Advertisement Data
28-byte public keys broadcast every 2 seconds, enabling passive Apple device detection [8].

### 3.3 Location Reports
Encrypted payloads with latitude, longitude, accuracy, timestamp, battery status. Only private key holders decrypt [9].

### 3.4 Apple ID Credentials
Username, password, 2FA for GSA/iCloud authentication. Tokens enable future requests [10].

### 3.5 Anisette Device Metadata
Provisioning headers mimicking Apple devices for iCloud access [11].

### 3.6 System Configuration
Configuration files, authentication tokens, server logs [12].

### 3.7 User Interaction Data
Frontend settings, device names, location cache [13].

## 4. Design and Implementation Choices

### 4.1 Architecture Overview

The system employs a three-tier architecture separating concerns across device firmware, backend services, and frontend applications:

```
┌─────────────────────────────────────────────────────────────────────┐
│                        MACLESS-HAYSTACK SYSTEM                       │
└─────────────────────────────────────────────────────────────────────┘

┌──────────────────┐         ┌────────────────────────────────────────┐
│   BLE DEVICE     │         │        APPLE FIND MY NETWORK           │
│   LAYER          │         │                                        │
│                  │         │  ┌──────────────┐    ┌──────────────┐ │
│ ┌──────────────┐ │         │  │ Apple Device │    │   iCloud     │ │
│ │  ESP32/NRF5x │ │◄────────┤  │  (iPhone/    │───►│   Servers    │ │
│ │   Firmware   │ │  BLE    │  │  iPad/Mac)   │    │              │ │
│ │              │ │  Advert │  └──────────────┘    └──────┬───────┘ │
│ │ - Key Storage│ │         │                             │         │
│ │ - Broadcast  │ │         │  Millions of devices        │         │
│ └──────────────┘ │         │  passively listening        │         │
└──────────────────┘         └─────────────────────────────┼─────────┘
                                                            │
        ┌───────────────────────────────────────────────────┼─────────┐
        │                  BACKEND LAYER                    │         │
        │                                                    ▼         │
        │  ┌────────────────────┐      ┌──────────────────────────┐  │
        │  │  Anisette Server   │◄─────┤  Macless-Haystack       │  │
        │  │                    │      │  Backend Endpoint        │  │
        │  │  - Device          │      │                          │  │
        │  │    Provisioning    │      │  - Apple Authentication  │  │
        │  │  - Header          │      │  - Report Fetching       │  │
        │  │    Generation      │      │  - HTTP API Server       │  │
        │  └────────────────────┘      └──────────┬───────────────┘  │
        │         Docker Container               │                    │
        └────────────────────────────────────────┼────────────────────┘
                                                 │ HTTP/HTTPS
                                                 │
        ┌────────────────────────────────────────┼────────────────────┐
        │              FRONTEND LAYER            │                    │
        │                                        ▼                    │
        │  ┌─────────────────────────────────────────────────────┐   │
        │  │         Flutter Application (Web/Mobile)            │   │
        │  │                                                      │   │
        │  │  ┌──────────────┐  ┌─────────────┐  ┌────────────┐ │   │
        │  │  │  UI Layer    │  │  Reports    │  │  Decryption│ │   │
        │  │  │              │  │  Fetcher    │  │  Engine    │ │   │
        │  │  │ - Map View   │◄─┤             │◄─┤            │ │   │
        │  │  │ - Settings   │  │ - HTTP      │  │ - ECDH     │ │   │
        │  │  │ - Device Mgmt│  │   Client    │  │ - AES-GCM  │ │   │
        │  │  └──────────────┘  └─────────────┘  └────────────┘ │   │
        │  │                                                      │   │
        │  │  Local Storage: Keys, Settings, Cache               │   │
        │  └─────────────────────────────────────────────────────┘   │
        │                                                             │
        └─────────────────────────────────────────────────────────────┘

        ┌─────────────────────────────────────────────────────────────┐
        │                   KEY GENERATION TOOL                        │
        │                                                              │
        │  ┌──────────────────────────────────────────────────────┐   │
        │  │  generate_keys.py (Python)                           │   │
        │  │                                                       │   │
        │  │  Input: Prefix, Number of Keys                       │   │
        │  │  Output:                                             │   │
        │  │    - PREFIX_keyfile (for firmware flashing)          │   │
        │  │    - PREFIX_devices.json (for frontend import)       │   │
        │  │    - PREFIX.keys (human-readable backup)             │   │
        │  └──────────────────────────────────────────────────────┘   │
        └─────────────────────────────────────────────────────────────┘

Figure 1: Technical Architecture of Macless-Haystack System
```

### 4.2 Key Technical Decisions

#### Elliptic Curve Cryptography (SECP224R1)
Uses SECP224R1 for Apple compatibility, providing 112-bit security with 28-byte keys suitable for BLE constraints [14]. *Assumption*: Apple chose this curve for security-bandwidth balance.

#### Dockerized Backend
Separate Docker containers for endpoint and Anisette provide isolation, portability, and reproducibility [15].

#### Anisette Dependency
External Anisette server [16] reduces maintenance but creates trust dependencies. Users can self-host for privacy.

#### Client-Side vs. Server-Side Decryption
Current default sends private keys to backend (simpler but requires trust). Client-side is recommended but requires complex Dart/JavaScript cryptography [17]. *Assumption*: Users prioritize ease over optimal security.

#### Flutter Frontend
Single codebase for web, Android, iOS reduces development effort and ensures consistent UI [18].

#### Static vs. Rotating Keys
Static keys by default, optional 30-minute rotation [19]. *Trade-off*: Simplicity over advanced privacy.

#### HTTP with Optional HTTPS
Supports both HTTP (local) and HTTPS (remote) [20]. HTTP is vulnerable per LINDDUN analysis.

### 4.3 Data Flow Pipeline

1. **Offline Phase**: User generates keypairs, flashes firmware with public keys, imports configuration to frontend
2. **Broadcasting**: BLE device broadcasts advertisement key every 2 seconds
3. **Collection**: Apple devices detect broadcasts, encrypt location with ephemeral keys, upload to iCloud
4. **Authentication**: Backend authenticates with Apple using Anisette-generated metadata
5. **Retrieval**: Backend queries iCloud with hashed advertisement keys, retrieves encrypted reports
6. **Decryption**: Frontend/backend performs ECDH key exchange, derives AES-GCM key, decrypts location
7. **Visualization**: Decrypted coordinates displayed on map with history timeline [21]

### 4.4 Security and Privacy

The system inherits Apple's end-to-end encryption but introduces trust requirements: endpoint operators, Anisette operators, and potential Apple EULA violations. The LINDDUN threat analysis identifies 27 privacy/security threats, several Critical severity [22,23].

## 5. Big Data Analytics Component

The system demonstrates big data characteristics: **Volume** - billions of daily reports from millions of devices [24]; **Velocity** - near-real-time generation; **Variety** - structured coordinates, timing patterns, metadata; **Veracity** - variable accuracy.

Potential extensions: movement pattern analysis, battery optimization, geofencing alerts, trajectory prediction.

## 6. Assumptions and Limitations

Due to Apple's proprietary systems, several details are reverse-engineered:

1. **Protocol Stability**: Apple's Find My protocol will remain stable [25]
2. **Account Restrictions**: Apple won't systematically block Macless-Haystack usage
3. **Network Coverage**: Sufficient Apple device density for effective tracking
4. **Legal Compliance**: Users accept EULA violation responsibility [26]
5. **Cryptographic Implementation**: Open-source Apple protocol implementations are correct [27]

## 7. Conclusion

Macless-Haystack democratizes Apple's Find My network access without expensive hardware. The modular architecture effectively separates concerns, though security/privacy trade-offs remain challenging. The project serves practical purposes (affordable tracking) and research goals (understanding proprietary location systems).

**Word Count**: ~1,446 words

## References

[1] dchristl/macless-haystack GitHub Repository, https://github.com/dchristl/macless-haystack

[2] generate_keys.py source code, uses Python cryptography library for SECP224R1 key generation

[3] ESP32 and NRF5x firmware implementations in firmware/ directory

[4] Apple Find My Network Accessory Specification (limited public documentation)

[5] DATAFLOW.md - "Apple devices passively receive the BLE advertisements"

[6] dadoum/anisette-v3-server GitHub Repository, https://github.com/Dadoum/anisette-v3-server

[7] DATAFLOW.md - Key Data Structures section, describes encryption workflow

[8] README.md - Hardware setup section, describes BLE advertisement frequency

[9] DATAFLOW.md - Location Report Payload structure

[10] pypush_gsa_icloud.py - GSA authentication implementation (biemster/FindMy project)

[11] DATAFLOW.md - Anisette Headers description

[12] FAQ.md - Configuration and authentication file storage

[13] macless_haystack/ Flutter application source code

[14] NIST FIPS 186-4 - Digital Signature Standard, SECP224R1 curve specification

[15] README.md - Docker setup instructions

[16] Anisette server integration in endpoint/mh_endpoint.py

[17] DATAFLOW.md - LINDDUN threat analysis, DD.3.a and L.2.2.1

[18] Flutter documentation, https://flutter.dev/multi-platform

[19] ESP32 firmware README.md - describes key rotation for battery optimization

[20] FAQ.md - SSL/HTTPS configuration section

[21] DATAFLOW.md - Detailed Data Flow Description, Phase 4

[22] DATAFLOW.md - Security Considerations section

[23] DATAFLOW.md - Privacy and Security Threat Analysis (LINDDUN) section

[24] Estimate based on Apple's 2+ billion active devices (Apple Q1 2024 earnings) and Find My network participation

[25] Assumption: Apple has not publicly documented commitment to protocol stability

[26] LICENSE file (AGPL v3) and README.md disclaimer section

[27] biemster/FindMy GitHub Repository, https://github.com/biemster/FindMy
