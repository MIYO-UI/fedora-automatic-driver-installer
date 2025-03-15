# Automatic Graphics Driver Installer for Fedora

This tool automatically detects and installs the recommended graphics drivers for your hardware on Fedora Linux systems. It's designed to work similarly to the automatic driver installation tools found in Windows.

## Features

- Automatically detects NVIDIA, AMD, and Intel graphics cards
- Installs the recommended proprietary or open-source drivers based on hardware
- Creates backups of existing configurations
- Tests the installed drivers and automatically rolls back to default drivers if issues are detected
- Provides both interactive and automatic (service-based) modes
- Integrates with the Fedora desktop environment

## Installation

### From Source

1. Ensure you have the required dependencies:
   ```bash
   sudo dnf install gcc-c++ cmake make pciutils
   ```

2. Clone the repository:
   ```bash
   git clone https://github.com/yourusername/auto-driver-installer.git
   cd auto-driver-installer
   ```

3. Build and install:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   sudo make install
   ```

4. (Optional) Enable the service to run automatically on next boot:
   ```bash
   sudo systemctl enable auto-driver-installer.service
   ```

### From RPM Package

```bash
sudo dnf install auto-driver-installer-1.0.0-1.fc*.rpm
```

## Usage

### GUI Mode

Simply launch "Graphics Driver Installer" from your application menu, or run:

```bash
sudo auto-driver-installer
```

The application will:
1. Detect your graphics hardware
2. Show you the recommended drivers
3. Ask for confirmation before installation
4. Install the appropriate drivers
5. Test the installation
6. Offer to reboot your system

### Command-Line Options

- `--auto`: Run in automatic mode without user interaction
- `--install-service`: Install and enable the systemd service

### Service Mode

To install and enable the service for automatic driver installation on first boot:

```bash
sudo auto-driver-installer --install-service
```

## Supported Hardware

- **NVIDIA**: GeForce and Quadro series using proprietary NVIDIA drivers
- **AMD**: Radeon series using open-source amdgpu drivers
- **Intel**: Integrated graphics using open-source intel drivers

## How It Works

1. **Detection**: Uses PCI information to identify installed graphics hardware
2. **Repository Setup**: Enables RPM Fusion repositories if needed
3. **Backup**: Creates backups of current graphics configurations
4. **Installation**: Installs appropriate drivers based on detected hardware
5. **Testing**: Verifies the system remains functional after driver installation
6. **Rollback**: Automatically reverts to default drivers if issues are detected

## Troubleshooting

Logs are stored in `/var/lib/driver-installer/install.log`. Check this file if you experience any issues.

## License

This project is licensed under the GNU General Public License v3.0.