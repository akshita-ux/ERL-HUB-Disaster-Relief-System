#ifndef ERL_HUB_HPP
#define ERL_HUB_HPP

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ReliefOps {
namespace Logistics { class IncidentCommander; }
namespace Inventory {

class CargoOverweightException : public std::exception {
    std::string message;
public:
    explicit CargoOverweightException(const std::string& m) : message(m) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class ReliefCargo {
    std::string name_;
    int quantity_;
    double unitWeight_;
    int priority_;
public:
    ReliefCargo(std::string name = "General Supplies", int quantity = 0,
                double unitWeight = 1.0, int priority = 3);
    const std::string& name() const noexcept { return name_; }
    int quantity() const noexcept { return quantity_; }
    double unitWeight() const noexcept { return unitWeight_; }
    int priority() const noexcept { return priority_; }
    double totalWeight() const noexcept { return quantity_ * unitWeight_; }
    void add(int quantity);
    void deduct(int quantity);
    friend std::ostream& operator<<(std::ostream&, const ReliefCargo&);
    friend std::istream& operator>>(std::istream&, ReliefCargo&);
    friend class ReliefOps::Logistics::IncidentCommander;
};

class DistributionDepot {
    std::string depotName_;
    std::map<std::string, ReliefCargo> stock_;
    static int depotCount_;
public:
    explicit DistributionDepot(std::string name = "Central Depot");
    DistributionDepot(const DistributionDepot&) = default;
    ~DistributionDepot();
    static int depotCount() noexcept { return depotCount_; }
    const std::string& name() const noexcept { return depotName_; }
    void addInitialStock(const ReliefCargo&);
    ReliefCargo& stock(const std::string& item);
    const ReliefCargo& stock(const std::string& item) const;
    DistributionDepot& operator+=(const ReliefCargo&);
    void allocate(const std::string& item, int quantity);
    void display(std::ostream&) const;
};

class Manifest {
    std::string manifestId_;
    ReliefCargo* items_;
    std::size_t size_;
    std::size_t capacity_;
    void reserve(std::size_t);
public:
    explicit Manifest(std::string id = "M-000");
    Manifest(const Manifest&);
    Manifest& operator=(const Manifest&);
    Manifest(Manifest&&) noexcept;
    Manifest& operator=(Manifest&&) noexcept;
    ~Manifest();
    void add(const ReliefCargo&);
    const ReliefCargo* data() const noexcept { return items_; }
    std::size_t size() const noexcept { return size_; }
    const std::string& id() const noexcept { return manifestId_; }
    double totalWeight() const noexcept;
    void deduct(const std::string& item, int quantity);
    bool operator==(const Manifest&) const noexcept;
    friend std::ostream& operator<<(std::ostream&, const Manifest&);
};

} // Inventory

namespace Logistics {

class FuelDepletionException : public std::exception {
    std::string message;
public:
    explicit FuelDepletionException(const std::string& m) : message(m) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class DamagedCorridorException : public std::exception {
    std::string message;
public:
    explicit DamagedCorridorException(const std::string& m) : message(m) {}
    const char* what() const noexcept override { return message.c_str(); }
};

class GPSBeacon {
    double latitude_, longitude_;
public:
    GPSBeacon(double lat = 28.6139, double lon = 77.2090) : latitude_(lat), longitude_(lon) {}
    void update(double lat, double lon) noexcept { latitude_ = lat; longitude_ = lon; }
    double latitude() const noexcept { return latitude_; }
    double longitude() const noexcept { return longitude_; }
};

class EmergencyLifeSupportUnit {
    bool operational_;
    int medicalKits_;
public:
    EmergencyLifeSupportUnit(bool operational = true, int kits = 10)
        : operational_(operational), medicalKits_(kits) {}
    bool operational() const noexcept { return operational_; }
    int medicalKits() const noexcept { return medicalKits_; }
    void consumeKit();
};

class ReliefVehicle {
protected:
    std::string vehicleId_;
    double capacityKg_;
    double fuel_;
    GPSBeacon gps_;                         // containership
    EmergencyLifeSupportUnit lifeSupport_; // containership
    static int operationalVehicles_;
    static double dispatchedTonnage_;
public:
    ReliefVehicle(std::string id, double capacity, double fuel,
                  GPSBeacon gps = GPSBeacon(),
                  EmergencyLifeSupportUnit life = EmergencyLifeSupportUnit());
    ReliefVehicle(const ReliefVehicle&) = default;
    virtual ~ReliefVehicle();
    virtual std::string type() const = 0;
    virtual double evaluateRoute(double distanceKm) const = 0;
    virtual void dispatch(double cargoWeightKg, double distanceKm);
    const std::string& id() const noexcept { return vehicleId_; }
    double capacity() const noexcept { return capacityKg_; }
    double fuel() const noexcept { return fuel_; }
    static int operationalVehicles() noexcept { return operationalVehicles_; }
    static double dispatchedTonnage() noexcept { return dispatchedTonnage_; }
    static void resetAnalytics() noexcept { dispatchedTonnage_ = 0.0; }
};

class GroundConvoy : virtual public ReliefVehicle {
public:
    GroundConvoy(std::string id, double capacity, double fuel);
    std::string type() const override { return "Ground Convoy"; }
    double evaluateRoute(double distanceKm) const override;
};

class AerialTransport : virtual public ReliefVehicle {
public:
    AerialTransport(std::string id, double capacity, double fuel);
    std::string type() const override { return "Aerial Transport"; }
    double evaluateRoute(double distanceKm) const override;
};

class AmphibiousCraft : public GroundConvoy, public AerialTransport {
public:
    AmphibiousCraft(std::string id, double capacity, double fuel);
    std::string type() const override { return "Amphibious Craft"; }
    double evaluateRoute(double distanceKm) const override;
};

class IncidentCommander {
public:
    void emergencyTriageOverride(Inventory::ReliefCargo&, int newPriority);
};

class TransportVehicle {
    std::unique_ptr<ReliefVehicle> vehicle_;
    Inventory::Manifest manifest_;
public:
    explicit TransportVehicle(std::unique_ptr<ReliefVehicle> vehicle,
                              Inventory::Manifest manifest = Inventory::Manifest());
    TransportVehicle(TransportVehicle&&) noexcept = default;
    TransportVehicle& operator=(TransportVehicle&&) noexcept = default;
    TransportVehicle(const TransportVehicle&) = delete;
    TransportVehicle& operator=(const TransportVehicle&) = delete;
    ReliefVehicle& vehicle() { return *vehicle_; }
    const ReliefVehicle& vehicle() const { return *vehicle_; }
    const Inventory::Manifest& manifest() const noexcept { return manifest_; }
    void operator-=(const Inventory::ReliefCargo& cargo);
    void dispatch(double distanceKm);
};

inline double calculateDistance(double x1, double y1, double x2, double y2) {
    const double dx = x2 - x1, dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

inline double calculatePayloadVolume(double length, double width, double height) {
    return length * width * height;
}

double& adjustMissionPriority(double& priority, double increment);
double allocateSupply(Inventory::DistributionDepot&, const std::string&, int quantity);
double allocateSupply(Inventory::DistributionDepot&, const std::string&, int quantity,
                      int emergencyPriority);

struct Mission {
    int id;
    std::string destination;
    std::string supply;
    int quantity;
    int priority; // 1 = most urgent
    double distanceKm;
};

struct UrgencyFirst {
    bool operator()(const Mission& a, const Mission& b) const {
        return a.priority < b.priority;
    }
};

template <typename T>
class PriorityDispatchQueue {
    std::vector<T> queue_;
public:
    void push(const T& item) { queue_.push_back(item); }
    bool empty() const noexcept { return queue_.empty(); }
    std::size_t size() const noexcept { return queue_.size(); }
    T pop() {
        if (queue_.empty()) throw std::out_of_range("Dispatch queue is empty.");
        auto it = std::min_element(queue_.begin(), queue_.end(),
            [](const T& a, const T& b) { return a.priority < b.priority; });
        T result = *it;
        queue_.erase(it);
        return result;
    }
    void sortByPriority() { std::sort(queue_.begin(), queue_.end(), UrgencyFirst()); }
    void display(std::ostream& out) const {
        for (const auto& item : queue_)
            out << std::setw(5) << item.id << std::setw(22) << item.destination
                << std::setw(12) << item.supply << std::setw(8) << item.quantity
                << std::setw(9) << item.priority << '\n';
    }
};

struct IncidentRecord {
    char tag[8];
    int missionId;
    int priority;
    double distanceKm;
    double cargoKg;
    char destination[64];
};

class BinaryIncidentLogger {
    std::string filename_;
public:
    explicit BinaryIncidentLogger(std::string filename = "erl_incidents.bin")
        : filename_(std::move(filename)) {}
    bool append(const Mission&, double cargoKg);
    std::vector<IncidentRecord> readAll() const;
    const std::string& filename() const noexcept { return filename_; }
};

struct TableHeader {};
inline TableHeader tableHeader() { return {}; }
std::ostream& operator<<(std::ostream&, TableHeader);

void runApplication();

} // Logistics
} // ReliefOps

#endif
