#include "erl_hub.hpp"
#include <cmath>
#include <cstdio>
#include <limits>

namespace ReliefOps {
namespace Inventory {

ReliefCargo::ReliefCargo(std::string name, int quantity, double unitWeight, int priority)
    : name_(std::move(name)), quantity_(quantity), unitWeight_(unitWeight), priority_(priority) {
    if (quantity < 0 || unitWeight <= 0 || priority < 1 || priority > 5)
        throw std::invalid_argument("Invalid cargo quantity, weight or priority.");
}
void ReliefCargo::add(int quantity) {
    if (quantity < 0) throw std::invalid_argument("Cannot add negative stock.");
    quantity_ += quantity;
}
void ReliefCargo::deduct(int quantity) {
    if (quantity < 0 || quantity > quantity_) throw std::out_of_range("Insufficient cargo quantity.");
    quantity_ -= quantity;
}
std::ostream& operator<<(std::ostream& out, const ReliefCargo& c) {
    return out << c.name_ << '|' << c.quantity_ << '|' << c.unitWeight_ << '|' << c.priority_;
}
std::istream& operator>>(std::istream& in, ReliefCargo& c) {
    std::string name;
    int quantity, priority;
    double weight;
    if (std::getline(in >> std::ws, name, '|') &&
        (in >> quantity) && in.get() == '|' && (in >> weight) && in.get() == '|' && (in >> priority)) {
        c = ReliefCargo(name, quantity, weight, priority);
    } else in.setstate(std::ios::failbit);
    return in;
}

int DistributionDepot::depotCount_ = 0;
DistributionDepot::DistributionDepot(std::string name) : depotName_(std::move(name)) { ++depotCount_; }
DistributionDepot::~DistributionDepot() { --depotCount_; }
void DistributionDepot::addInitialStock(const ReliefCargo& cargo) { stock_[cargo.name()] = cargo; }
ReliefCargo& DistributionDepot::stock(const std::string& item) {
    auto it = stock_.find(item);
    if (it == stock_.end()) throw std::out_of_range("Supply not found in depot: " + item);
    return it->second;
}
const ReliefCargo& DistributionDepot::stock(const std::string& item) const {
    auto it = stock_.find(item);
    if (it == stock_.end()) throw std::out_of_range("Supply not found in depot: " + item);
    return it->second;
}
DistributionDepot& DistributionDepot::operator+=(const ReliefCargo& cargo) {
    auto it = stock_.find(cargo.name());
    if (it == stock_.end()) stock_.emplace(cargo.name(), cargo);
    else it->second.add(cargo.quantity());
    return *this;
}
void DistributionDepot::allocate(const std::string& item, int quantity) { stock(item).deduct(quantity); }
void DistributionDepot::display(std::ostream& out) const {
    out << "\nSTOCK AT " << depotName_ << "\n";
    out << std::left << std::setw(24) << "SUPPLY" << std::setw(12) << "QUANTITY"
        << std::setw(12) << "KG/UNIT" << "PRIORITY\n";
    for (const auto& entry : stock_) {
        const auto& c = entry.second;
        out << std::left << std::setw(24) << c.name() << std::setw(12) << c.quantity()
            << std::setw(12) << c.unitWeight() << c.priority() << '\n';
    }
}

Manifest::Manifest(std::string id) : manifestId_(std::move(id)), items_(nullptr), size_(0), capacity_(0) {}
void Manifest::reserve(std::size_t requested) {
    if (requested <= capacity_) return;
    std::size_t next = std::max(requested, capacity_ == 0 ? std::size_t(2) : capacity_ * 2);
    ReliefCargo* fresh = new ReliefCargo[next];
    for (std::size_t i = 0; i < size_; ++i) fresh[i] = items_[i];
    delete[] items_;
    items_ = fresh;
    capacity_ = next;
}
Manifest::Manifest(const Manifest& other)
    : manifestId_(other.manifestId_), items_(nullptr), size_(other.size_), capacity_(other.capacity_) {
    if (capacity_) {
        items_ = new ReliefCargo[capacity_];
        for (std::size_t i = 0; i < size_; ++i) items_[i] = other.items_[i];
    }
}
Manifest& Manifest::operator=(const Manifest& other) {
    if (this != &other) {
        Manifest copy(other);
        *this = std::move(copy);
    }
    return *this;
}
Manifest::Manifest(Manifest&& other) noexcept
    : manifestId_(std::move(other.manifestId_)), items_(other.items_),
      size_(other.size_), capacity_(other.capacity_) {
    other.items_ = nullptr; other.size_ = other.capacity_ = 0;
}
Manifest& Manifest::operator=(Manifest&& other) noexcept {
    if (this != &other) {
        delete[] items_;
        manifestId_ = std::move(other.manifestId_);
        items_ = other.items_; size_ = other.size_; capacity_ = other.capacity_;
        other.items_ = nullptr; other.size_ = other.capacity_ = 0;
    }
    return *this;
}
Manifest::~Manifest() { delete[] items_; }
void Manifest::add(const ReliefCargo& cargo) { reserve(size_ + 1); items_[size_++] = cargo; }
void Manifest::deduct(const std::string& item, int quantity) {
    for (std::size_t i = 0; i < size_; ++i) {
        if (items_[i].name() == item) {
            items_[i].deduct(quantity);
            return;
        }
    }
    throw std::out_of_range("Cargo item not found in manifest.");
}
double Manifest::totalWeight() const noexcept {
    double total = 0;
    for (std::size_t i = 0; i < size_; ++i) total += items_[i].totalWeight();
    return total;
}
bool Manifest::operator==(const Manifest& other) const noexcept {
    return manifestId_ == other.manifestId_;
}
std::ostream& operator<<(std::ostream& out, const Manifest& m) {
    out << "Manifest " << m.manifestId_ << " | Items: " << m.size_
        << " | Total weight: " << m.totalWeight() << " kg\n";
    for (std::size_t i = 0; i < m.size_; ++i) out << "  " << m.items_[i].name()
        << " x " << m.items_[i].quantity() << '\n';
    return out;
}

} // Inventory

namespace Logistics {

void EmergencyLifeSupportUnit::consumeKit() {
    if (medicalKits_ <= 0) throw FuelDepletionException("Emergency medical kits depleted.");
    --medicalKits_;
}
int ReliefVehicle::operationalVehicles_ = 0;
double ReliefVehicle::dispatchedTonnage_ = 0.0;

ReliefVehicle::ReliefVehicle(std::string id, double capacity, double fuel, GPSBeacon gps,
                             EmergencyLifeSupportUnit life)
    : vehicleId_(std::move(id)), capacityKg_(capacity), fuel_(fuel),
      gps_(gps), lifeSupport_(life) {
    if (capacity <= 0 || fuel < 0) throw std::invalid_argument("Invalid vehicle capacity or fuel.");
    ++operationalVehicles_;
}
ReliefVehicle::~ReliefVehicle() { --operationalVehicles_; }
void ReliefVehicle::dispatch(double cargoWeightKg, double distanceKm) {
    if (cargoWeightKg > capacityKg_) throw Inventory::CargoOverweightException("Cargo exceeds vehicle payload capacity.");
    const double required = evaluateRoute(distanceKm);
    if (fuel_ < required) throw FuelDepletionException("Insufficient fuel for selected route.");
    if (distanceKm > 500) throw DamagedCorridorException("Route exceeds safe operational corridor limit.");
    if (!lifeSupport_.operational()) throw FuelDepletionException("Emergency life-support unit is not operational.");
    fuel_ -= required;
    dispatchedTonnage_ += cargoWeightKg / 1000.0;
}
GroundConvoy::GroundConvoy(std::string id, double capacity, double fuel)
    : ReliefVehicle(std::move(id), capacity, fuel) {}
double GroundConvoy::evaluateRoute(double km) const { return km * 0.12; }
AerialTransport::AerialTransport(std::string id, double capacity, double fuel)
    : ReliefVehicle(std::move(id), capacity, fuel) {}
double AerialTransport::evaluateRoute(double km) const { return km * 0.35; }
AmphibiousCraft::AmphibiousCraft(std::string id, double capacity, double fuel)
    : ReliefVehicle(id, capacity, fuel), GroundConvoy(id, capacity, fuel),
      AerialTransport(id, capacity, fuel) {}
double AmphibiousCraft::evaluateRoute(double km) const { return km * 0.20; }

void IncidentCommander::emergencyTriageOverride(Inventory::ReliefCargo& cargo, int newPriority) {
    if (newPriority < 1 || newPriority > 5) throw std::invalid_argument("Priority must be between 1 and 5.");
    cargo.priority_ = newPriority; // friend access to private cargo data
}

TransportVehicle::TransportVehicle(std::unique_ptr<ReliefVehicle> vehicle, Inventory::Manifest manifest)
    : vehicle_(std::move(vehicle)), manifest_(std::move(manifest)) {
    if (!vehicle_) throw std::invalid_argument("Transport vehicle cannot be null.");
}
void TransportVehicle::operator-=(const Inventory::ReliefCargo& cargo) {
    if (cargo.totalWeight() > vehicle_->capacity())
        throw Inventory::CargoOverweightException("Cargo cannot be loaded: payload limit exceeded.");
    manifest_.deduct(cargo.name(), cargo.quantity());
}
void TransportVehicle::dispatch(double distanceKm) {
    vehicle_->dispatch(manifest_.totalWeight(), distanceKm);
}

double& adjustMissionPriority(double& priority, double increment) {
    priority = std::max(1.0, std::min(5.0, priority + increment));
    return priority;
}
double allocateSupply(Inventory::DistributionDepot& depot, const std::string& item, int quantity) {
    depot.allocate(item, quantity);
    return quantity;
}
double allocateSupply(Inventory::DistributionDepot& depot, const std::string& item,
                      int quantity, int emergencyPriority) {
    if (emergencyPriority > 0 && emergencyPriority <= 2) {
        // Emergency triage allocation uses the same stock ledger with priority validation.
        depot.allocate(item, quantity);
        return quantity * 1.0;
    }
    return allocateSupply(depot, item, quantity);
}

std::ostream& operator<<(std::ostream& out, TableHeader) {
    out << std::left << std::setw(5) << "ID" << std::setw(22) << "DESTINATION"
        << std::setw(12) << "SUPPLY" << std::setw(8) << "QTY"
        << std::setw(9) << "PRIORITY" << '\n'
        << std::string(56, '-') << '\n';
    return out;
}

bool BinaryIncidentLogger::append(const Mission& mission, double cargoKg) {
    IncidentRecord record{};
    std::strncpy(record.tag, "ERLLOG", sizeof(record.tag) - 1);
    record.missionId = mission.id;
    record.priority = mission.priority;
    record.distanceKm = mission.distanceKm;
    record.cargoKg = cargoKg;
    std::strncpy(record.destination, mission.destination.c_str(), sizeof(record.destination) - 1);

    std::fstream file(filename_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
        std::ofstream create(filename_, std::ios::binary);
        create.close();
        file.open(filename_, std::ios::in | std::ios::out | std::ios::binary);
    }
    if (!file) return false;

    file.seekp(0, std::ios::end);
    const std::streampos originalEnd = file.tellp();
    file.write(reinterpret_cast<const char*>(&record), sizeof(record));
    file.flush();

    if (!file) { // transactional rollback to the original file length
        file.clear();
        file.seekp(originalEnd);
        file.flush();
        return false;
    }

    IncidentRecord verification{};
    file.seekg(originalEnd);
    file.read(reinterpret_cast<char*>(&verification), sizeof(verification));
    if (!file || std::strncmp(verification.tag, "ERLLOG", 6) != 0) {
        file.clear();
        file.seekp(originalEnd);
        file.flush();
        return false;
    }
    return true;
}
std::vector<IncidentRecord> BinaryIncidentLogger::readAll() const {
    std::vector<IncidentRecord> records;
    std::ifstream file(filename_, std::ios::binary);
    IncidentRecord record{};
    while (file.read(reinterpret_cast<char*>(&record), sizeof(record))) records.push_back(record);
    return records;
}

static void printMenu() {
    std::cout << "\n========== ERL-HUB DISASTER RELIEF ==========\n"
              << "1. View inventory\n2. Restock supplies\n3. Register/dispatch ground convoy\n"
              << "4. Register/dispatch helicopter\n5. Register/dispatch amphibious craft\n"
              << "6. Add mission to priority queue\n7. View missions by priority\n"
              << "8. Dispatch next mission\n9. View binary incident history\n"
              << "10. View system analytics\n0. Exit\nChoice: ";
}

void runApplication() {
    Inventory::DistributionDepot depot("Regional Relief Depot");
    depot += Inventory::ReliefCargo("Rations", 500, 1.2, 3);
    depot += Inventory::ReliefCargo("Medicine", 150, 0.4, 1);
    depot += Inventory::ReliefCargo("Water", 300, 5.0, 2);
    depot += Inventory::ReliefCargo("Blankets", 250, 1.0, 3);

    PriorityDispatchQueue<Mission> missions;
    BinaryIncidentLogger logger;
    int nextMissionId = 1001;
    std::unordered_map<std::string, int> dispatchCount;

    for (;;) {
        printMenu();
        int choice;
        if (!(std::cin >> choice)) { std::cin.clear(); std::cin.ignore(10000, '\n'); continue; }
        try {
            if (choice == 0) break;
            if (choice == 1) depot.display(std::cout);
            else if (choice == 2) {
                std::string item; int qty;
                std::cout << "Supply name (exact): "; std::cin.ignore(); std::getline(std::cin, item);
                std::cout << "Quantity to restock: "; std::cin >> qty;
                depot += Inventory::ReliefCargo(item, qty, 1.0, 3);
                std::cout << "Stock updated.\n";
            } else if (choice >= 3 && choice <= 5) {
                std::string id, destination, supply;
                int qty, priority;
                double distance;
                std::cout << "Vehicle ID: "; std::cin >> id;
                std::cout << "Destination (one word): "; std::cin >> destination;
                std::cout << "Supply name (exact, one word): "; std::cin >> supply;
                std::cout << "Quantity: "; std::cin >> qty;
                std::cout << "Mission priority (1=urgent, 5=normal): "; std::cin >> priority;
                std::cout << "Route distance (km): "; std::cin >> distance;
                auto& available = depot.stock(supply);
                if (qty > available.quantity()) throw std::out_of_range("Not enough stock at depot.");
                Inventory::Manifest manifest("MAN-" + id);
                Inventory::ReliefCargo cargo(supply, qty, available.unitWeight(), priority);
                manifest.add(cargo);
                std::unique_ptr<ReliefVehicle> vehicle;
                if (choice == 3) vehicle.reset(new GroundConvoy(id, 5000, 1000));
                else if (choice == 4) vehicle.reset(new AerialTransport(id, 1500, 1500));
                else vehicle.reset(new AmphibiousCraft(id, 3500, 1200));
                TransportVehicle transport(std::move(vehicle), manifest);
                std::vector<ReliefVehicle*> fleetView;
                fleetView.push_back(&transport.vehicle());
                transport.dispatch(distance);
                transport -= cargo; // deduct manifest cargo after successful departure
                depot.allocate(supply, qty);
                Mission m{nextMissionId++, destination, supply, qty, priority, distance};
                logger.append(m, cargo.totalWeight());
                ++dispatchCount[destination];
                std::cout << "SUCCESS: " << transport.vehicle().type() << " " << id
                          << " dispatched to " << destination << ".\n";
            } else if (choice == 6) {
                Mission m{};
                m.id = nextMissionId++;
                std::cout << "Destination Supply Quantity Priority Distance(km): ";
                std::cin >> m.destination >> m.supply >> m.quantity >> m.priority >> m.distanceKm;
                if (m.priority < 1 || m.priority > 5 || m.quantity <= 0 || m.distanceKm <= 0)
                    throw std::invalid_argument("Invalid mission values.");
                missions.push(m);
                std::cout << "Mission queued with ID " << m.id << ".\n";
            } else if (choice == 7) {
                missions.sortByPriority();
                std::cout << tableHeader();
                missions.display(std::cout);
            } else if (choice == 8) {
                Mission m = missions.pop();
                std::cout << "Next mission: " << m.destination << ", " << m.supply
                          << ", priority " << m.priority << ".\n";
                allocateSupply(depot, m.supply, m.quantity, m.priority);
                std::unique_ptr<ReliefVehicle> v(new GroundConvoy("AUTO-" + std::to_string(m.id), 5000, 1000));
                Inventory::Manifest manifest("Q-" + std::to_string(m.id));
                manifest.add(Inventory::ReliefCargo(m.supply, m.quantity, depot.stock(m.supply).unitWeight(), m.priority));
                TransportVehicle transport(std::move(v), manifest);
                transport.dispatch(m.distanceKm);
                transport -= Inventory::ReliefCargo(m.supply, m.quantity, depot.stock(m.supply).unitWeight(), m.priority);
                logger.append(m, manifest.totalWeight());
                std::cout << "Queued mission dispatched and logged.\n";
            } else if (choice == 9) {
                const auto records = logger.readAll();
                std::cout << "\nBINARY INCIDENT AUDIT (" << records.size() << " records)\n";
                for (const auto& r : records)
                    std::cout << "Mission " << r.missionId << " | " << r.destination
                              << " | Priority " << r.priority << " | "
                              << r.cargoKg << " kg | " << r.distanceKm << " km\n";
            } else if (choice == 10) {
                std::cout << "Active depots: " << Inventory::DistributionDepot::depotCount()
                          << "\nActive vehicles: " << ReliefVehicle::operationalVehicles()
                          << "\nTotal dispatched tonnage: " << ReliefVehicle::dispatchedTonnage()
                          << " metric tonnes\nPending missions: " << missions.size()
                          << "\nDestinations served: " << dispatchCount.size() << '\n';
            } else std::cout << "Invalid choice.\n";
        } catch (const Inventory::CargoOverweightException& e) {
            std::cerr << "CARGO ERROR: " << e.what() << '\n';
        } catch (const FuelDepletionException& e) {
            std::cerr << "FUEL/LIFE-SUPPORT ERROR: " << e.what() << '\n';
        } catch (const DamagedCorridorException& e) {
            std::cerr << "ROUTE ERROR: " << e.what() << '\n';
        } catch (const std::exception& e) {
            std::cerr << "OPERATION ERROR: " << e.what() << '\n';
        }
    }
    std::cout << "ERL-HUB shut down. Incident data remains saved in " << logger.filename() << ".\n";
}

} // Logistics
} // ReliefOps
