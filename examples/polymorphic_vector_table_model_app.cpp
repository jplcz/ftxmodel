#include <ftxmodel/polymorphic_vector_table_model.hpp>
#include <iostream>

struct SystemProcess {
  std::string name;
  int pid;
};
struct NetworkSocket {
  std::string protocol;
  int port;
};

int main() {
  using namespace ftxmodel;

  auto model = std::make_unique<PolymorphicVectorTableModel>();
  model->declareColumn("Entity Identifier");
  model->declareColumn("Network/PID Metric");

  // ========================================================================
  // Register SystemProcess
  // ========================================================================
  model->registerTypeHandler<SystemProcess>(
      [](const std::any& a, int col, ItemRole r) -> std::any {
        if (r != ItemRole::DisplayRole) {
          return {};
        }
        const auto& proc = std::any_cast<const SystemProcess&>(a);

        if (col == 0) {
          return proc.name;
        }
        if (col == 1) {
          return "PID: " + std::to_string(proc.pid);
        }
        return {};
      });

  // ========================================================================
  // Register NetworkSocket
  // ========================================================================
  model->registerTypeHandler<NetworkSocket>(
      [](const std::any& a, int col, ItemRole r) -> std::any {
        if (r != ItemRole::DisplayRole) {
          return {};
        }
        const auto& sock = std::any_cast<const NetworkSocket&>(a);

        if (col == 0) {
          return "Socket bound via " + sock.protocol;
        }
        if (col == 1) {
          return "Port: " + std::to_string(sock.port);
        }
        return {};
      });

  // Hydrate heterogeneous entries
  model->appendRowItem(SystemProcess{"init-system", 1});
  model->appendRowItem(NetworkSocket{"TCP", 8080});

  // View queries execute uniformly across columns
  std::cout << "Row 0, Col 0: " << model->textData(model->index(0, 0))
            << "\n";  // init-system
  std::cout << "Row 1, Col 1: " << model->textData(model->index(1, 1))
            << "\n";  // Port: 8080

  return 0;
}
