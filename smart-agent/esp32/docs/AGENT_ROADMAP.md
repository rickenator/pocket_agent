# Agentic Roadmap - Smart Agent ESP32

## Overview

This document outlines the roadmap for transforming the Smart Agent ESP32 (Waveshare model) from a passive voice assistant into an **autonomous, goal-oriented agent** - similar to a tiny Clawdbot. The Waveshare device provides high-resolution touch screen and function button, enabling rich visual feedback and energy-efficient interactions.

**Hardware Capabilities:**
- High-resolution touch screen (4.3" or 7" display)
- Function button (push button for actions)
- WiFi connectivity
- Microphone for voice input
- Speaker for TTS output
- Battery power with power management

**Key Features for Agent Development:**
1. **High-Resolution Display** - Rich visuals, avatars, maps, and text output
2. **Touch Screen** - Interactive UI and button functionality
3. **Function Button** - Push-to-talk, quick actions, wake-up
4. **Visual Feedback** - Avatar states, battery indicators, real-time status
5. **Battery Optimization** - Energy-efficient interactions

An agent should not just respond to commands but actively pursue goals, use tools, make decisions, and persist information.

## Definition: What Makes an Agent "Agentic"?

> "An agent is a system that can perceive its environment, take actions to achieve goals, and learn from experiences to improve future performance."

Key characteristics of an autonomous agent:

1. **Goal-Oriented** - Has explicit goals and works toward them autonomously
2. **Tool Use** - Can use various tools and APIs to accomplish tasks
3. **Decision Making** - Can make decisions based on context and goals
4. **Memory & Persistence** - Remembers past experiences and state
5. **Action Execution** - Can execute physical actions (not just voice)
6. **Self-Correction** - Can adapt and learn from failures
7. **Multi-Modal** - Works with voice, text, sensors, actuators
8. **Autonomy** - Works independently with minimal human intervention

## Hardware Capabilities

### Waveshare Device Features

#### Display System
- **High-Resolution Touch Screen** (4.3" 480x272 or 7" 800x480)
- **Full Graphics Support** - Draw images, icons, text, shapes
- **Multi-Page Content** - Scrollable text, images, maps
- **High-DPI Rendering** - Clear visuals at any size
- **Touch Input** - Interactive UI elements

#### Input/Output
- **Function Button** (Push button)
  - Single press: Wake from sleep, show status
  - Hold: Push-to-talk mode (for battery saving)
  - Double press: Quick action selection

- **Voice Input**
  - Microphone for voice commands
  - Noise cancellation support
  - Energy-efficient wake-up detection

- **Voice Output**
  - Speaker for TTS
  - Adjustable volume and speed
  - Audio playback queue

#### Power System
- **Battery Management**
  - LiPo battery support
  - Power consumption monitoring
  - Sleep/wake modes
  - Battery life indicators

### Display Capabilities for Agent Feedback

#### Avatar System
**Visual State Indicators:**
- **Listening State** (❌): Listening for ряд commands
- **Processing State** (⟳): Processing query or thinking
- **Talking State** (o): Speaking response
- **Idle State** (—): Waiting for input
- **Sleep State** (z): Low power mode

**Battery Status Indicators:**
- **High Battery** (100%): Full charge, green
- **Moderate Battery** (50%): Good, yellow
- **Low Battery** (20%): Caution, orange
- **Critical Battery** (10%): Emergency, red blinking

**Activity Indicators:**
- **Recording** (●): Recording voice input
- **Streaming** (⠋): Streaming response from LLM
- **WiFi Connected** (W): Network active
- **Bluetooth Connected** (B): Device connected
- **Updating** (⟳): Downloading or updating

#### Content Types
1. **Text Display**
   - Long-form text with scrolling
   - Code blocks with syntax highlighting
   - Markdown support

2. **Visual Content**
   - Animated avatars
   - Icons and emojis
   - Maps and charts
   - Images and photos

3. **UI Components**
   - Progress bars (loading, charging)
   - Buttons and controls
   - Status badges
   - Navigation menus

4. **Status Dashboard**
   - Current mode (Voice, Text, Auto)
   - Connected devices
   - Task queue
   - System health metrics


| Capability | Current Status | Agent Readiness |
|------------|----------------|-----------------|
| Voice Recognition | ✅ Simulation | Basic |
| Text-to-Speech | ❌ Not Implemented | Missing |
| WiFi Management | ✅ Complete | ✅ Complete |
| Web Interface | ✅ Provisioning | ✅ Basic |
| LLM Integration | ⚠️ Simulation | 🚧 In Progress |
| Model Selection | ❌ Not Implemented | Missing |
| Display Output | ⚠️ Simulation | 🚧 Basic |
| High-Res Display | ❌ Not Implemented | Missing |
| Touch Screen | ⚠️ Simulation | 🚧 Basic |
| Avatar System | ❌ Not Implemented | Missing |
| Push-to-Talk | ❌ Not Implemented | Missing |
| Function Button | ⚠️ Simulation | 🚧 Basic |
| Persistent Storage | ❌ Not Implemented | Missing |
| Tool Use | ❌ Not Implemented | Missing |
| Goal Management | ❌ Not Implemented | Missing |
| Action Execution | ❌ Not Implemented | Missing |
| Sensor Integration | ❌ Not Implemented | Missing |

## Phase 1: Foundation (Weeks 1-2)

### 1.1 Add Persistent Memory
- Implement NVS storage for:
  - WiFi credentials (already partially done)
  - Conversation history
  - User preferences
  - Task queue
  - Agent state

**Tasks:**
- [ ] Create `MemoryManager` component
- [ ] Implement NVS wrapper for key-value storage
- [ ] Add JSON serialization for complex data
- [ ] Implement memory persistence on shutdown

**Deliverable:** NVS-based memory system with save/load capabilities

### 1.2 Implement Text-to-Speech (TTS)
- Add TTS for voice responses
- Support multiple languages/voices
- Volume and speed control

**Tasks:**
- [ ] Integrate ESP-SR or external TTS service
- [ ] Add TTS API to AudioDriver
- [ ] Implement async TTS queue
- [ ] Add speech synthesis callback

**Deliverable:** TTS system with queue management

### 1.3 Complete LLM Integration
- Finish Ollama HTTP client implementation
- Add streaming support
- Implement JSON response parsing
- Add retry logic and timeout handling
- Add multi-source model support (Ollama + Cloud APIs)

**Tasks:**
- [ ] Implement esp_http_client wrapper
- [ ] Add cJSON JSON parsing
- [ ] Implement SSE streaming parser
- [ ] Add error handling and recovery
- [ ] Add model selection API supporting multiple sources
- [ ] Integrate Ollama model listing and selection
- [ ] Add free-tier cloud model support (Gemini, NVIDIA)
- [ ] Implement model provider abstraction layer
- [ ] Add model comparison and selection UI

**Model Sources:**
1. **Ollama (Local)** - Privacy-first, offline, no API costs
   - llama2, llama3, mistral, phi2, phi3
   - Custom models
   - Quantized versions (q4_0, q4_K_M, etc.)

2. **Google Gemini Flash (Cloud)** - Free tier available
   - gemini-1.5-flash (fast, free)
   - gemini-pro
   - Multi-modal support

3. **NVIDIA (Cloud)** - Free tier available
   - Llama 2 70B
   - Mistral 7B
   - Other models

4. **OpenAI (Cloud)** - Paid, requires API key
   - gpt-3.5-turbo
   - gpt-4

**Model Selection UI:**
- Home screen: Show available models with status
- Model cards: Size, speed, capabilities, provider
- Quick select: Favorite models on home screen
- Preferences: Default model, auto-switch based on task

**Deliverable:** Fully functional LLM integration with multi-source support

### 1.4 Goal Management System
- Define agent goals and subtasks
- Implement goal scheduling
- Add goal priority queue

**Tasks:**
- [ ] Define goal types (short-term, long-term)
- [ ] Create `GoalManager` component
- [ ] Implement goal execution loop
- [ ] Add goal persistence

**Deliverable:** Goal management system with execution framework

### 1.5 Push-to-Talk Implementation
- Implement function button as push-to-talk for battery saving
- Add wake-up on button press
- Implement voice command capture mode
- Add energy-efficient sleep/wake cycle

**Tasks:**
- [ ] Configure function button GPIO
- [ ] Add interrupt handler for button press
- [ ] Implement wake-from-sleep on button press
- [ ] Add push-to-talk mode activation
- [ ] Capture voice commands without screen wake-up
- [ ] Add button press debouncing
- [ ] Implement battery-saving sleep modes

**Battery Savings:**
- **Sleep Mode**: Battery drain ~0.1mA (vs ~20mA idle)
- **Wake-on-Press**: Only power up when needed
- **No Screen Wake**: Keep screen off during listening
- **Optimized Audio**: Lower volume, shorter silence detection

**Use Cases:**
1. **Hands-Free Operation**: Speak commands without waking display
2. **Battery Preservation**: Extended battery life during sleep
3. **Quick Actions**: Double press for quick menu
4. **Emergency Wake**: Wake device when in deep sleep

**Deliverable:** Push-to-talk with significant battery savings (60-80%)

## Phase 2: Tool Use & Capabilities (Weeks 3-4)

### 2.1 Tool Framework
- Define tool types and interfaces
- Implement tool registration system
- Add tool execution context

**Tool Types to Implement:**
1. **Information Tools**
   - Web search API integration
   - Weather API integration
   - Time/date lookup
   - Calculator

2. **Control Tools**
   - Device control (GPIO)
   - IoT device communication
   - Scheduling

3. **Utility Tools**
   - File operations
   - System information
   - Network diagnostics

**Tasks:**
- [ ] Create `Tool` base class
- [ ] Define tool types and interfaces
- [ ] Implement tool registry
- [ ] Add tool execution framework
- [ ] Implement web search tool
- [ ] Implement weather tool
- [ ] Implement GPIO control tool

**Deliverable:** Extensible tool framework with working examples

### 2.2 Sensor Integration
- Add sensor interfaces
- Implement data collection
- Create sensor event system

**Sensors to Consider:**
- Temperature/humidity sensors (DHT11/22)
- Motion sensors (PIR)
- Light sensors
- Sound sensors
- Battery level
- Accelerometer/magnetometer

**Tasks:**
- [ ] Create `SensorManager` component
- [ ] Add sensor driver interfaces
- [ ] Implement sensor polling
- [ ] Create sensor event system
- [ ] Add data visualization on display

**Deliverable:** Sensor integration framework with example drivers

### 2.3 High-Resolution Display Implementation
- Implement Waveshare display driver (ST7789 or ILI9341 for 4.3"/7" screens)
- Add high-resolution graphics library
- Create comprehensive UI framework
- Implement touch screen support

**Tasks:**
- [ ] Add display driver for Waveshare panel (4.3" 480x272 or 7" 800x480)
- [ ] Implement full-color RGB graphics library
- [ ] Add font rendering (multiple sizes and styles)
- [ ] Implement image loading and display
- [ ] Create scrolling text and multi-page content
- [ ] Add touch screen driver and input handling
- [ ] Create UI widgets (buttons, progress bars, sliders)
- [ ] Implement navigation menu system
- [ ] Add visual effects (animations, transitions)
- [ ] Optimize display refresh rate and power usage

**Display Features:**
- **Full Color**: 262K colors, rich visuals
- **High Resolution**: 480x272 or 800x480 pixels
- **Text Support**: Multiple fonts, sizes, and styles
- **Graphics**: Lines, circles, rectangles, polygons
- **Images**: BMP, PNG support
- **Animations**: Smooth transitions and effects
- **Touch Interaction**: Multi-touch support
- **Power Saving**: Dimming and sleep modes

**Content Types:**
- Text: Markdown, code blocks, structured content
- Images: User photos, generated graphics
- Maps: Interactive map display
- Avatars: Animated characters
- Charts: Data visualization
- Icons: Emoji and custom icons

**Deliverable:** High-resolution touch display with rich visual capabilities

## Phase 3: Autonomy & Decision Making (Weeks 5-6)

### 3.1 Agent Decision Engine
- Implement reasoning and planning
- Add context awareness
- Create action selection system

**Tasks:**
- [ ] Create `ReasoningEngine` component
- [ ] Implement rule-based decision making
- [ ] Add probabilistic decision support
- [ ] Implement context memory
- [ ] Add action priority system

**Deliverable:** Decision engine with rule-based reasoning

### 3.2 Task Planning
- Implement task decomposition
- Create subtask execution
- Add task monitoring and adjustment

**Tasks:**
- [ ] Create `TaskPlanner` component
- [ ] Implement task decomposition algorithm
- [ ] Add task state tracking
- [ ] Implement task retry logic
- [ ] Add self-correction mechanisms

**Deliverable:** Task planning system with execution tracking

### 3.3 Learning & Adaptation
- Implement experience storage
- Add learning from feedback
- Create personalization

**Tasks:**
- [ ] Create `LearningEngine` component
- [ ] Implement feedback collection
- [ ] Add experience replay
- [ ] Implement preference learning
- [ ] Create agent personality profiles

**Deliverable:** Learning system with adaptation capabilities

## Phase 4: Action Execution (Weeks 7-8)

### 4.1 Physical Action System
- Define action types
- Implement action queue
- Add action execution monitoring

**Action Types:**
1. **Voice Actions**
   - TTS output
   - Voice command

2. **Display Actions**
   - Show text
   - Show graphics
   - Update UI

3. **System Actions**
   - Reboot
   - WiFi reconnect
   - Device reset

4. **External Actions** (via web server)
   - Control other devices
   - Send commands

**Tasks:**
- [ ] Create `ActionManager` component
- [ ] Define action types and interfaces
- [ ] Implement action queue
- [ ] Add action monitoring
- [ ] Implement action retry
- [ ] Add action safety limits

**Deliverable:** Action execution system with queue and monitoring

### 4.2 Web Server Enhancements
- Add REST API for remote control
- Implement WebSocket for real-time updates
- Create dashboard UI
- Add authentication

**Tasks:**
- [ ] Add REST endpoints for all actions
- [ ] Implement WebSocket server
- [ ] Create web dashboard
- [ ] Add authentication
- [ ] Implement rate limiting

**Deliverable:** Full-featured web interface with API

### 4.3 IoT Device Integration
- Add MQTT client
- Create device management
- Implement device automation

**Tasks:**
- [ ] Add MQTT client component
- [ ] Create device registry
- [ ] Implement device control
- [ ] Add automation rules
- [ ] Create device state monitoring

**Deliverable:** IoT device integration with automation

## Phase 5: Advanced Features (Weeks 9-10)

### 5.1 Natural Language Understanding
- Implement intent recognition
- Add slot filling
- Create dialogue management

**Tasks:**
- [ ] Integrate intent classifier
- [ ] Implement slot extraction
- [ ] Create dialogue state manager
- [ ] Add multi-turn conversation handling
- [ ] Implement context window

**Deliverable:** Advanced NLU system

### 5.2 Safety & Ethics
- Implement safety checks
- Add content filtering
- Create user consent system

**Tasks:**
- [ ] Add safety filters
- [ ] Implement content validation
- [ ] Create user consent workflow
- [ ] Add usage tracking
- [ ] Implement privacy controls

**Deliverable:** Safety and ethics framework

### 5.3 Optimization & Performance
- Implement resource management
- Add power optimization
- Create monitoring system

**Tasks:**
- [ ] Add resource usage monitoring
- [ ] Implement power saving modes
- [ ] Optimize memory usage
- [ ] Add performance profiling
- [ ] Create diagnostic tools

**Deliverable:** Optimized and efficient agent

### 5.4 Mobile Integration
- Create mobile companion app
- Add BLE communication
- Implement remote control

**Tasks:**
- [ ] Design mobile app interface
- [ ] Implement BLE communication
- [ ] Create app server endpoints
- [ ] Add push notifications
- [ ] Implement app analytics

**Deliverable:** Mobile companion application

## Phase 6: Testing & Deployment (Weeks 11-12)

### 6.1 Comprehensive Testing
- Unit testing
- Integration testing
- User acceptance testing

**Tasks:**
- [ ] Create test framework
- [ ] Add unit tests for all components
- [ ] Implement integration tests
- [ ] Add end-to-end tests
- [ ] Create test scenarios

**Deliverable:** Comprehensive test suite

### 6.2 Documentation
- Complete API documentation
- Create user guide
- Add developer documentation
- Create troubleshooting guide

**Tasks:**
- [ ] Document all APIs
- [ ] Write user guide
- [ ] Create developer documentation
- [ ] Add troubleshooting guide
- [ ] Create video tutorials

**Deliverable:** Complete documentation

### 6.3 Deployment
- Create firmware images
- Set up OTA updates
- Create deployment scripts
- Document deployment process

**Tasks:**
- [ ] Build production firmware
- [ ] Implement OTA update system
- [ ] Create deployment automation
- [ ] Document setup process
- [ ] Create support contact

**Deliverable:** Production-ready deployment

### 6.4 Avatar System Implementation

## Future Work & Advanced Features

### High-Resolution Display Capabilities

The Waveshare device's high-resolution display enables rich visual experiences beyond basic text:

#### Image Generation and Display
- **AI-Generated Images**: Display images createdكي by LLMs
- **Maps and Navigation**: Interactive map display with location markers
- **Visual Documentation**: Display user photos and custom graphics
- **Generated Art**: Display AI-generated images for creative tasks

#### Advanced Avatar Features
- **Emotion-Based Expressions**: Avatar changes face based on sentiment
- **Dynamic Animations**: Smooth, context-aware animations
- **Multi-Avatar Support**: Different personalities for different moods
- **User-Customizable Avatars**: Allow users to upload their own avatar
- **Avatar Learning**: Avatar adapts based on user interactions and preferences

#### Visual Content Types
- **Rich Media**: Images, icons, and graphics
- **Data Visualization**: Charts, graphs, and statistics
- **Code Display**: Syntax-highlighted code blocks
- **Video Content**: Limited video playback for important notifications
- **Interactive UI**: Buttons, sliders, and navigation controls

### Advanced Push-to-Talk Features

1. **Voice Command Templates**
   - Pre-configured voice commands for quick actions
   - Custom command creation via web interface
   - Command history and suggestions

2. **Smart Wake-up**
   - Voice-activated wake-up with keyword detection
   - Motion sensor wake-up integration
   - Scheduled wake-up cycles

3. **Battery Optimization**
   - Intelligent sleep scheduling
   - Usage pattern learning
   - Power consumption analysis

4. **Multi-Device Coordination**
   - Wake-up other nearby devices
   - Voice command propagation
   - Device state synchronization

### Visual Feedback Enhancements

1. **Real-Time Status Dashboard**
   - Current mode display (Voice, Text, Auto)
   - Connected devices status
   - Active tasks and progress
   - System health metrics

2. **Gesture Recognition**
   - Tap, swipe, pinch gestures
   - Custom gesture commands
   - Haptic feedback

3. **Audio-Visual Synchronization**
   - Avatar lip-sync with TTS
   - Visual indicators for audio events
   - Animated sound effects

4. **Interactive Storytelling**
   - Agent narratives
   - Context-aware storytelling
   - User participatory experiences

### Cloud Integration Enhancements

1. **Multi-Cloud Support**
   - Google, NVIDIA, OpenAI, Anthropic
   - Automatic provider selection
   - Fallback and redundancy

2. **Personalized Models**
   - User preference learning
   - Custom model fine-tuning
   - Adaptive model selection

3. **Continuous Learning**
   - User behavior analysis
   - Preference optimization
   - Context-aware responses

### Advanced Agent Capabilities

1. **Multi-Agent Collaboration**
   - Task delegation between agents
   - Agent specialization
   - Cooperative problem-solving

2. **Advanced Memory**
   - Long-term memory with episodic recall
   - Semantic memory for concepts
   - Procedural memory for skills

3. **Self-Improvement**
   - Automatic performance tuning
   - Error recovery and learning
   - Continuous optimization

### 6.4 Avatar System Implementation
- Create animated avatar for visual feedback
- Implement state-based animations
- Add battery and status indicators
- Create interactive visual elements

**Tasks:**
- [ ] Design avatar system with state machine
- [ ] Create SVG animations for states (listening, talking, idle, sleep)
- [ ] Implement battery level visualization
- [ ] Add status indicators (WiFi, sensors, tasks)
- [ ] Create visual feedback for agent actions
- [ ] Implement smooth state transitions
- [ ] Add user avatar customization
- [ ] Create character personality traits

**Avatar States:**
1. **Listening State** (❌)
   - Ears animated, head tilted
   - Pulsing indicator
   - "Listening..." text

2. **Processing State** (⟳)
   - Thinking animation
   - Brain or gears icon
   - "Processing..." text

3. **Talking State** (o)
   - Animated mouth
   - Speech bubbles
   - "Speaking..." indicator

4. **Idle State** (—)
   - Relaxed posture
   - Blinking animation
   - Status summary

5. **Sleep State** (z)
   - Zzz animation
   - Power-off icon
   - "Sleeping..." text

**Battery Status Visuals:**
- **High (>80%)**: Full green bar, happy face
- **Moderate (50-80%)**: Yellow bar, neutral face
- **Low (20-50%)**: Orange bar, concerned face
- **Critical (<20%)**: Red blinking bar, urgent icon

**Status Indicators:**
- Recording indicator (●) when mic is active
- Streaming indicator (⠋) during LLM response
- WiFi/WiFi Connected (W) when network active
- Task in progress (⟳) with progress bar

**Future Enhancements:**
- User-uploaded avatars
- Dynamic expressions based on sentiment
- Avatar learning user preferences
- Multi-avatar support for different personalities

## Success Metrics

### Functional Metrics
- [ ] Agent can autonomously achieve 90% of user goals
- [ ] System response time < 2 seconds for common queries
- [ ] 95% uptime with automatic recovery
- [ ] Battery life > 24 hours (for battery-powered devices)

### User Metrics
- [ ] User satisfaction > 4/5
- [ ] Agent successfully learns 80% of user preferences
- [ ] Reduces user intervention by 70%

### Technical Metrics
- [ ] Memory usage < 50% of available RAM
- [ ] CPU usage < 30% average
- [ ] OTA update success rate > 99%
- [ ] Zero security vulnerabilities

## Example Agentic Workflow

Here's an example of how an agent should behave:

**User:** "I'm going on a trip tomorrow. Make sure I have everything ready."

**Agent:** "I'll help you prepare for your trip. Let me check the weather, remind you about your schedule, and ensure your devices are ready."

1. **Goal:** Prepare user for trip
2. **Subtasks:**
   - Get weather forecast for destination
   - Check user's calendar for tomorrow
   - Verify devices are charged
   - Set reminders

3. **Actions:**
   - Call weather API
   - Query calendar
   - Check battery levels
   - Set alarms

4. **Feedback & Adjustment:**
   - "Weather looks good (72°F, sunny)"
   - "You have a meeting at 10 AM"
   - "Phone is at 85% battery"
   - "I've set reminders for your flight"

**Agent:** "Your trip is all set! Good luck! 🎒✈️"

## Technology Stack Recommendations

### Core Framework
- ESP-IDF 5.x (latest)
- FreeRTOS for concurrency
- ESP32-C3 or ESP32-S3 for better AI capabilities
- **Waveshare Touch Display** (4.3" 480x272 or 7" 800x480)
- **Function Button** for push-to-talk and wake-up

### LLM Integration
- **Ollama** (local LLM server) - Privacy-first, offline
- **llama2, llama3, mistral, phi2, phi3** - Local models
- **Free Cloud APIs** (optional):
  - Google Gemini Flash - Free tier available
  - NVIDIA AI Enterprise - Free tier available
  - OpenAI GPT-3.5/GPT-4 - Paid API

### Display & Graphics
- **ST7789/ILI9341** driver for Waveshare screens
- **Full-color RGB graphics library**
- **Font rendering** - Multiple sizes and styles
- **Image support** - BMP, PNG formats
- **Touch screen driver** - Input handling and gestures
- **UI framework** - Widgets, navigation, animations

### Communication
- WiFi for web/MQTT
- BLE for mobile pairing
- HTTP/HTTPS for REST APIs
- **Function button GPIO** for wake-up and actions

### Storage
- NVS for preferences and session data
- SPIFFS/LittleFS for large files and user data
- **Conversation history persistence**

### Additional Libraries
- cJSON for JSON processing
- ESP Async WebServer for web interface
- MQTT client for IoT devices
- ESP-SR for speech recognition (optional)
- **AVG or free-avatar graphics** for visual feedback

## Conclusion

This roadmap provides a clear path to transform the Smart Agent ESP32 (Waveshare device) from a basic voice assistant into a true autonomous agent. Each phase builds upon the previous, creating a comprehensive, capable system that can understand, reason, act, and learn.

**Key Differentiators:**
- **High-Resolution Touch Display** - Rich visuals for user interaction
- **Push-to-Talk** - Energy-efficient voice commands via function button
- **Avatar System** - Visual feedback for state and battery status
- **Multi-Source LLM** - Flexible model selection (Ollama + cloud APIs)
- **Autonomous Operation** - Goal-driven behavior with persistent memory

The key is to start with foundational capabilities (memory, TTS, LLM) and gradually add autonomy (goal management, tool use) before focusing on advanced features (learning, IoT, mobile).

**Technology Highlights:**
- ✅ **Local-first** - Privacy with Ollama models
- ✅ **Flexible** - Switch to cloud APIs when needed
- ✅ **Energy-efficient** - Push-to-talk for battery savings
- ✅ **Visual** - Avatar and high-res display for feedback
- ✅ **Future-proof** - Scalable architecture for growth

**Estimated Timeline:** 12 weeks
**Total Effort:** ~4-6 person-months
**Budget:** Depends on hardware choices and development team size

*Note: This roadmap is a guide and should be adjusted based on actual progress, resource availability, and stakeholder feedback.*