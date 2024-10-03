## Overview

ImgFS is an image-oriented file system designed for efficient management and retrieval of images. Inspired by Facebook's Haystack system, ImgFS allows users to interact with images through a command-line interface (CLI) and an HTTP server. The primary goal is to provide a simple way to store, access, and manage images while offering a familiar user experience for both CLI and web clients.

## Implementation

### Features

- **Command-Line Interface (CLI)**: Users can execute commands to:
  - List images
  - Add images
  - Delete images
  - Extract images

- **HTTP Server**: The server mimics the functionalities of the CLI and offers an HTTP interface for interaction.

- **JSON Support**: The `list` command provides output in JSON format, making it easier for web applications to consume the data.

- **Multi-threading**: The server can handle multiple connections simultaneously, improving responsiveness and performance.

### Structure

- **Core Components**:
  - **imgfscmd**: Command-line utility for managing images.
  - **HTTP Server**: Handles incoming requests and serves responses.

- **Client Code**: An example client is provided in `index.html`, written in JavaScript, for testing the server functionality. Additionally, `curl` can be used for command-line testing.

- **Unit and End-to-End Tests**:
  - New unit tests are included to ensure core functionalities work as expected.
  - End-to-end tests are provided to validate the server's behavior in real-world scenarios.
