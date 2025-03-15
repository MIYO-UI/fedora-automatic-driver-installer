#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cstdlib>
#include <regex>

// Dla operacji na procesach i plikach
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

namespace fs = std::filesystem;

// Struktura przechowująca informacje o urządzeniu graficznym
struct GraphicsDevice {
    std::string pciId;        // ID urządzenia PCI (np. 10de:1234)
    std::string vendor;       // Nazwa producenta (NVIDIA, AMD, Intel)
    std::string model;        // Model karty graficznej
    std::string currentDriver; // Aktualnie używany sterownik
    bool isPrimary;           // Czy to główna karta graficzna
};

// Enumeracja typów sterowników
enum class DriverType {
    NVIDIA_PROPRIETARY,
    NVIDIA_OPEN,
    NVIDIA_NOUVEAU,
    AMD_PROPRIETARY,
    AMD_OPEN,
    INTEL_OPEN,
    GENERIC,
    UNKNOWN
};

// Klasa do zarządzania sterownikami
class DriverManager {
private:
    std::vector<GraphicsDevice> detectedDevices;
    std::string backupDir = "/var/lib/driver-installer/backup";
    std::string logFile = "/var/lib/driver-installer/install.log";
    bool rpmFusionEnabled = false;
    
    // Mapa przypisująca ID producentów do nazw
    const std::map<std::string, std::string> vendorMap = {
        {"10de", "NVIDIA"},
        {"1002", "AMD"},
        {"8086", "Intel"}
    };

public:
    DriverManager() {
        // Utworzenie katalogów, jeśli nie istnieją
        if (!fs::exists("/var/lib/driver-installer")) {
            fs::create_directories("/var/lib/driver-installer");
        }
        if (!fs::exists(backupDir)) {
            fs::create_directories(backupDir);
        }
    }

    // Inicjalizacja i uruchomienie procesu instalacji
    bool initialize() {
        logMessage("Rozpoczęcie procesu automatycznej instalacji sterowników");
        
        // Sprawdzenie, czy skrypt jest uruchomiony z uprawnieniami roota
        if (geteuid() != 0) {
            logMessage("BŁĄD: Ten program musi być uruchomiony z uprawnieniami administratora (root)");
            return false;
        }
        
        // Wykrycie urządzeń graficznych
        if (!detectGraphicsDevices()) {
            logMessage("BŁĄD: Nie udało się wykryć urządzeń graficznych");
            return false;
        }
        
        // Utworzenie kopii zapasowej konfiguracji
        createBackup();
        
        // Sprawdzenie i włączenie repozytoriów
        enableRepositories();
        
        return true;
    }
    
    // Wykrywanie urządzeń graficznych
    bool detectGraphicsDevices() {
        logMessage("Wykrywanie urządzeń graficznych...");
        
        // Użycie komendy lspci do wykrywania kart graficznych
        std::string output = executeCommand("lspci -nn | grep -E 'VGA|3D|Display'");
        
        // Regex do parsowania wyniku lspci
        std::regex deviceRegex("(.+) VGA compatible controller.*\\[([0-9a-f]{4}):([0-9a-f]{4})\\](?:: (.+))?");
        std::smatch matches;
        std::istringstream stream(output);
        std::string line;
        
        while (std::getline(stream, line)) {
            if (std::regex_search(line, matches, deviceRegex)) {
                GraphicsDevice device;
                device.pciId = matches[2].str() + ":" + matches[3].str();
                
                // Określanie producenta
                auto vendorIt = vendorMap.find(matches[2].str());
                if (vendorIt != vendorMap.end()) {
                    device.vendor = vendorIt->second;
                } else {
                    device.vendor = "Unknown";
                }
                
                device.model = matches.size() > 4 ? matches[4].str() : "Unknown Model";
                device.isPrimary = detectedDevices.empty(); // Pierwsza wykryta karta jest traktowana jako główna
                
                // Sprawdzenie aktualnie używanego sterownika
                device.currentDriver = detectCurrentDriver(device.pciId);
                
                detectedDevices.push_back(device);
                logMessage("Wykryto urządzenie: " + device.vendor + " " + device.model + " [" + device.pciId + "]");
            }
        }
        
        return !detectedDevices.empty();
    }
    
    // Uruchomienie procesu instalacji sterowników
    bool installDrivers() {
        logMessage("Rozpoczęcie instalacji sterowników...");
        
        bool allSuccess = true;
        
        for (const auto& device : detectedDevices) {
            logMessage("Instalacja sterowników dla: " + device.vendor + " " + device.model);
            
            bool success = false;
            
            if (device.vendor == "NVIDIA") {
                success = installNvidiaDrivers(device);
            } else if (device.vendor == "AMD") {
                success = installAmdDrivers(device);
            } else if (device.vendor == "Intel") {
                success = installIntelDrivers(device);
            } else {
                logMessage("Nieznany producent karty graficznej. Pomijanie instalacji sterowników.");
                continue;
            }
            
            if (!success) {
                logMessage("OSTRZEŻENIE: Nie udało się zainstalować sterowników dla " + device.vendor + " " + device.model);
                allSuccess = false;
            }
        }
        
        if (allSuccess) {
            logMessage("Instalacja sterowników zakończona pomyślnie");
        } else {
            logMessage("Instalacja zakończona z ostrzeżeniami lub błędami");
        }
        
        return allSuccess;
    }
    
    // Testowanie zainstalowanych sterowników
    bool testDrivers() {
        logMessage("Testowanie zainstalowanych sterowników...");
        
        // W rzeczywistej implementacji moglibyśmy uruchomić testy wydajności lub stabilności
        // Uproszczona implementacja po prostu sprawdza, czy system nadal działa poprawnie
        
        // Sprawdzenie, czy proces X11 lub Wayland nadal działa
        bool displayServerRunning = false;
        
        if (system("pgrep -x Xorg > /dev/null") == 0 || 
            system("pgrep -x X > /dev/null") == 0 ||
            system("pgrep -x wayland > /dev/null") == 0) {
            displayServerRunning = true;
        }
        
        if (!displayServerRunning) {
            logMessage("BŁĄD: Serwer wyświetlania nie działa po instalacji sterowników!");
            return false;
        }
        
        // Dodatkowe testy można by dodać tutaj
        
        logMessage("Pomyślnie przetestowano sterowniki");
        return true;
    }
    
    // Przywracanie domyślnych sterowników w przypadku niepowodzenia
    bool restoreDefaultDrivers() {
        logMessage("Przywracanie domyślnych sterowników...");
        
        // Przywrócenie kopii zapasowej konfiguracji
        if (fs::exists(backupDir + "/xorg.conf") && fs::exists("/etc/X11/xorg.conf")) {
            executeCommand("cp " + backupDir + "/xorg.conf /etc/X11/xorg.conf");
        }
        
        for (const auto& device : detectedDevices) {
            if (device.vendor == "NVIDIA") {
                // Usunięcie sterownika NVIDIA i instalacja nouveau
                executeCommand("dnf remove -y akmod-nvidia xorg-x11-drv-nvidia*");
                executeCommand("dnf install -y xorg-x11-drv-nouveau");
            } else if (device.vendor == "AMD") {
                // Przywrócenie domyślnych sterowników radeon/amdgpu
                executeCommand("dnf reinstall -y mesa-dri-drivers mesa-libGL xorg-x11-drv-amdgpu");
            }
            // Dla Intela zwykle nie ma potrzeby przywracania, ponieważ używa się sterowników open source
        }
        
        logMessage("Przywrócono domyślne sterowniki");
        return true;
    }
    
    // Uzyskanie listy wykrytych urządzeń
    const std::vector<GraphicsDevice>& getDetectedDevices() const {
        return detectedDevices;
    }

private:
    // Wykrywanie aktualnie używanego sterownika
    std::string detectCurrentDriver(const std::string& pciId) {
        // Wyciągnięcie ID producenta z PCI ID
        std::string vendorId = pciId.substr(0, 4);
        
        if (vendorId == "10de") { // NVIDIA
            if (system("lsmod | grep -q nouveau") == 0) {
                return "nouveau";
            } else if (system("lsmod | grep -q nvidia") == 0) {
                return "nvidia";
            }
        } else if (vendorId == "1002") { // AMD
            if (system("lsmod | grep -q amdgpu") == 0) {
                return "amdgpu";
            } else if (system("lsmod | grep -q radeon") == 0) {
                return "radeon";
            }
        } else if (vendorId == "8086") { // Intel
            return "intel";
        }
        
        return "unknown";
    }
    
    // Instalacja sterowników NVIDIA
    bool installNvidiaDrivers(const GraphicsDevice& device) {
        logMessage("Instalacja sterowników NVIDIA...");
        
        // Sprawdzenie, czy RPM Fusion jest włączone
        if (!rpmFusionEnabled) {
            logMessage("BŁĄD: Repozytoria RPM Fusion nie są włączone. Nie można zainstalować sterowników NVIDIA.");
            return false;
        }
        
        // Instalacja sterowników NVIDIA
        int result = system("dnf install -y akmod-nvidia xorg-x11-drv-nvidia xorg-x11-drv-nvidia-cuda");
        
        if (result != 0) {
            logMessage("BŁĄD: Nie udało się zainstalować sterowników NVIDIA");
            return false;
        }
        
        // Odczekanie na skompilowanie modułu kernela
        logMessage("Oczekiwanie na zbudowanie modułu jądra NVIDIA...");
        std::this_thread::sleep_for(std::chrono::seconds(60));
        
        // Sprawdzenie, czy moduł został poprawnie skompilowany
        if (system("lsmod | grep -q nvidia") != 0) {
            logMessage("OSTRZEŻENIE: Moduł NVIDIA nie został załadowany");
            return false;
        }
        
        // Konfiguracja pliku xorg.conf
        system("nvidia-xconfig");
        
        logMessage("Pomyślnie zainstalowano sterowniki NVIDIA");
        return true;
    }
    
    // Instalacja sterowników AMD
    bool installAmdDrivers(const GraphicsDevice& device) {
        logMessage("Instalacja sterowników AMD...");
        
        // Instalacja sterowników AMDGPU i Mesa
        int result = system("dnf install -y mesa-dri-drivers mesa-libGL mesa-vulkan-drivers xorg-x11-drv-amdgpu");
        
        if (result != 0) {
            logMessage("BŁĄD: Nie udało się zainstalować sterowników AMD");
            return false;
        }
        
        // Konfiguracja dla AMD
        std::ofstream xorgConf("/etc/X11/xorg.conf.d/20-amdgpu.conf");
        if (xorgConf.is_open()) {
            xorgConf << "Section \"Device\"\n";
            xorgConf << "    Identifier \"AMD\"\n";
            xorgConf << "    Driver \"amdgpu\"\n";
            xorgConf << "    Option \"TearFree\" \"true\"\n";
            xorgConf << "EndSection\n";
            xorgConf.close();
        }
        
        logMessage("Pomyślnie zainstalowano sterowniki AMD");
        return true;
    }
    
    // Instalacja sterowników Intel
    bool installIntelDrivers(const GraphicsDevice& device) {
        logMessage("Instalacja sterowników Intel...");
        
        // Instalacja sterowników Intel
        int result = system("dnf install -y mesa-dri-drivers mesa-libGL xorg-x11-drv-intel");
        
        if (result != 0) {
            logMessage("BŁĄD: Nie udało się zainstalować sterowników Intel");
            return false;
        }
        
        // Konfiguracja dla Intel
        std::ofstream xorgConf("/etc/X11/xorg.conf.d/20-intel.conf");
        if (xorgConf.is_open()) {
            xorgConf << "Section \"Device\"\n";
            xorgConf << "    Identifier \"Intel Graphics\"\n";
            xorgConf << "    Driver \"intel\"\n";
            xorgConf << "    Option \"TearFree\" \"true\"\n";
            xorgConf << "EndSection\n";
            xorgConf.close();
        }
        
        logMessage("Pomyślnie zainstalowano sterowniki Intel");
        return true;
    }
    
    // Utworzenie kopii zapasowej aktualnej konfiguracji
    void createBackup() {
        logMessage("Tworzenie kopii zapasowej konfiguracji...");
        
        // Backup /etc/X11/xorg.conf jeśli istnieje
        if (fs::exists("/etc/X11/xorg.conf")) {
            executeCommand("cp /etc/X11/xorg.conf " + backupDir + "/xorg.conf");
        }
        
        // Backup katalogu xorg.conf.d
        if (fs::exists("/etc/X11/xorg.conf.d")) {
            executeCommand("mkdir -p " + backupDir + "/xorg.conf.d");
            executeCommand("cp -r /etc/X11/xorg.conf.d/* " + backupDir + "/xorg.conf.d/");
        }
        
        // Backup aktualnych modułów jądra
        executeCommand("lsmod > " + backupDir + "/lsmod.txt");
        
        logMessage("Kopia zapasowa została utworzona");
    }
    
    // Włączenie potrzebnych repozytoriów (RPM Fusion)
    void enableRepositories() {
        logMessage("Sprawdzanie i włączanie repozytoriów...");
        
        // Sprawdzenie, czy RPM Fusion jest już włączone
        if (system("dnf repolist | grep -q rpmfusion") == 0) {
            logMessage("Repozytoria RPM Fusion są już włączone");
            rpmFusionEnabled = true;
            return;
        }
        
        // Instalacja RPM Fusion Free
        int resultFree = system("dnf install -y https://mirrors.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm");
        
        // Instalacja RPM Fusion Non-free (potrzebne dla sterowników NVIDIA)
        int resultNonfree = system("dnf install -y https://mirrors.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm");
        
        if (resultFree == 0 && resultNonfree == 0) {
            logMessage("Pomyślnie włączono repozytoria RPM Fusion");
            rpmFusionEnabled = true;
        } else {
            logMessage("OSTRZEŻENIE: Nie udało się włączyć repozytoriów RPM Fusion");
        }
    }
    
    // Zapisywanie wiadomości do pliku logów
    void logMessage(const std::string& message) {
        // Wyświetlenie komunikatu na standardowe wyjście
        std::cout << message << std::endl;
        
        // Zapisanie do pliku logów
        std::ofstream log(logFile, std::ios_base::app);
        if (log.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto now_time_t = std::chrono::system_clock::to_time_t(now);
            log << std::ctime(&now_time_t) << message << std::endl;
            log.close();
        }
    }
    
    // Wykonanie komendy systemowej i zwrócenie jej wyjścia
    std::string executeCommand(const std::string& command) {
        std::array<char, 128> buffer;
        std::string result;
        
        // Wykonanie komendy i przechwycenie wyjścia
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
        if (!pipe) {
            return "";
        }
        
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        
        return result;
    }
};

// Klasa głównego programu
class AutoDriverInstaller {
private:
    DriverManager driverManager;
    
public:
    // Uruchomienie instalacji sterowników
    int run() {
        std::cout << "===== Automatyczna instalacja sterowników graficznych Fedora =====" << std::endl;
        std::cout << "Ten program automatycznie wykryje i zainstaluje zalecane sterowniki" << std::endl;
        std::cout << "dla Twojej karty graficznej. W przypadku niepowodzenia przywróci" << std::endl;
        std::cout << "domyślne sterowniki." << std::endl;
        std::cout << std::endl;
        
        // Inicjalizacja
        if (!driverManager.initialize()) {
            std::cerr << "Nie można kontynuować z powodu błędów inicjalizacji." << std::endl;
            return 1;
        }
        
        // Wyświetlenie wykrytych urządzeń
        const auto& devices = driverManager.getDetectedDevices();
        std::cout << "\nWykryte urządzenia graficzne:" << std::endl;
        for (const auto& device : devices) {
            std::cout << "- " << device.vendor << " " << device.model;
            std::cout << " [Aktualny sterownik: " << device.currentDriver << "]" << std::endl;
        }
        
        // Pytanie o zgodę użytkownika
        std::cout << "\nCzy chcesz kontynuować instalację zalecanych sterowników? (t/n): ";
        std::string response;
        std::getline(std::cin, response);
        
        if (response != "t" && response != "T") {
            std::cout << "Instalacja została anulowana przez użytkownika." << std::endl;
            return 0;
        }
        
        // Instalacja sterowników
        bool installSuccess = driverManager.installDrivers();
        
        if (!installSuccess) {
            std::cout << "\nWystąpiły problemy podczas instalacji sterowników." << std::endl;
            std::cout << "Czy chcesz przywrócić domyślne sterowniki? (t/n): ";
            std::getline(std::cin, response);
            
            if (response == "t" || response == "T") {
                driverManager.restoreDefaultDrivers();
                std::cout << "Przywrócono domyślne sterowniki." << std::endl;
            }
            
            return 1;
        }
        
        // Testowanie sterowników
        if (!driverManager.testDrivers()) {
            std::cout << "\nWystąpiły problemy z nowymi sterownikami." << std::endl;
            std::cout << "Przywracanie domyślnych sterowników..." << std::endl;
            
            driverManager.restoreDefaultDrivers();
            return 1;
        }
        
        std::cout << "\nInstalacja sterowników zakończona pomyślnie!" << std::endl;
        std::cout << "Zaleca się ponowne uruchomienie systemu, aby zmiany zostały w pełni zastosowane." << std::endl;
        std::cout << "Czy chcesz teraz ponownie uruchomić system? (t/n): ";
        std::getline(std::cin, response);
        
        if (response == "t" || response == "T") {
            system("reboot");
        }
        
        return 0;
    }
};

// Funkcja do tworzenia pliku usługi systemd
void createSystemdService() {
    std::ofstream serviceFile("/etc/systemd/system/auto-driver-installer.service");
    if (serviceFile.is_open()) {
        serviceFile << "[Unit]\n";
        serviceFile << "Description=Automatic Graphics Driver Installer\n";
        serviceFile << "After=network.target\n";
        serviceFile << "\n";
        serviceFile << "[Service]\n";
        serviceFile << "Type=oneshot\n";
        serviceFile << "ExecStart=/usr/bin/auto-driver-installer --auto\n";
        serviceFile << "RemainAfterExit=yes\n";
        serviceFile << "\n";
        serviceFile << "[Install]\n";
        serviceFile << "WantedBy=multi-user.target\n";
        serviceFile.close();
    }
}

// Tryb automatyczny (bez interakcji z użytkownikiem)
int runAutomatic() {
    DriverManager driverManager;
    
    // Inicjalizacja
    if (!driverManager.initialize()) {
        return 1;
    }
    
    // Instalacja sterowników
    bool installSuccess = driverManager.installDrivers();
    
    // Testowanie sterowników
    if (!installSuccess || !driverManager.testDrivers()) {
        driverManager.restoreDefaultDrivers();
        return 1;
    }
    
    return 0;
}

// Główna funkcja programu
int main(int argc, char* argv[]) {
    // Sprawdzenie, czy uruchomiono w trybie automatycznym
    if (argc > 1 && std::string(argv[1]) == "--auto") {
        return runAutomatic();
    }
    
    // Sprawdzenie, czy uruchomiono z opcją instalacji usługi
    if (argc > 1 && std::string(argv[1]) == "--install-service") {
        createSystemdService();
        system("systemctl enable auto-driver-installer.service");
        std::cout << "Usługa automatycznej instalacji sterowników została zainstalowana i włączona." << std::endl;
        return 0;
    }
    
    // Uruchomienie w trybie interaktywnym
    AutoDriverInstaller installer;
    return installer.run();
}