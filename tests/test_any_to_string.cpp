#include <gtest/gtest.h>
#include <ftxmodel/any_to_string.hpp>
#include <mutex>
#include <thread>

using namespace ftxmodel;

// MOCK USER CUSTOM TYPES
struct CustomUser {
  std::string username;
  int id;
};

struct Vector2D {
  float x;
  float y;
};

struct Point3D {
  int x, y, z;
};

struct DiagnosticReport {
  std::string status;
  double load_factor;
};

TEST(AnyToStringTranslatorTest, DynamicTemplateRegistration) {
  // 1. Register CustomUser formatting layout specifications
  AnyToStringTranslator::Register<CustomUser>([](const CustomUser& user) {
    return std::format("{} (ID: #{})", user.username, user.id);
  });

  // 2. Register Vector2D formatting layout specifications
  AnyToStringTranslator::Register<Vector2D>([](const Vector2D& vec) {
    return std::format("[X: {:.1f}, Y: {:.1f}]", vec.x, vec.y);
  });

  // 3. Act & Assert: Put instances into std::any and test parsing output
  CustomUser test_user{"Alice", 404};
  std::any any_user = test_user;
  EXPECT_EQ(AnyToStringTranslator::Translate(any_user), "Alice (ID: #404)");

  Vector2D test_vector{12.5f, -3.0f};
  std::any any_vector = test_vector;
  EXPECT_EQ(AnyToStringTranslator::Translate(any_vector), "[X: 12.5, Y: -3.0]");

  // 4. Clean up the engine spec configuration
  AnyToStringTranslator::Unregister<CustomUser>();

  // Once unregistered, it should drop down cleanly to the fallback string token
  EXPECT_EQ(AnyToStringTranslator::Translate(any_user, "UNKNOWN"), "UNKNOWN");
}

TEST(AnyToStringTranslatorTest, NumericBoundaryLimits) {
  // Large and Small Integral Limits
  EXPECT_EQ(AnyToStringTranslator::Translate(
                std::any(std::numeric_limits<int>::max())),
            std::to_string(std::numeric_limits<int>::max()));
  EXPECT_EQ(AnyToStringTranslator::Translate(
                std::any(std::numeric_limits<int>::min())),
            std::to_string(std::numeric_limits<int>::min()));

  EXPECT_EQ(AnyToStringTranslator::Translate(
                std::any(std::numeric_limits<unsigned long long>::max())),
            std::to_string(std::numeric_limits<unsigned long long>::max()));

  // Zero-padding float behaviors
  // std::to_string would produce "0.000000", std::format produces "0"
  EXPECT_EQ(AnyToStringTranslator::Translate(std::any(0.0)), "0");
  EXPECT_EQ(AnyToStringTranslator::Translate(std::any(-0.0)), "-0");
}

TEST(AnyToStringTranslatorTest, StringPointerSpecializations) {
  // Non-const char pointers
  char mutable_buffer[] = "DynamicBuffer";
  char* raw_char_ptr = mutable_buffer;
  EXPECT_EQ(AnyToStringTranslator::Translate(std::any(raw_char_ptr)),
            "DynamicBuffer");

  // ReSharper disable once CppLocalVariableMayBeConst
  const char* const_char_ptr = "ConstantBuffer";
  EXPECT_EQ(AnyToStringTranslator::Translate(std::any(const_char_ptr)),
            "ConstantBuffer");

  // Empty and null variants
  EXPECT_EQ(AnyToStringTranslator::Translate(std::any(std::string(""))), "");
  EXPECT_EQ(AnyToStringTranslator::Translate(std::any(std::string_view(""))),
            "");
}

TEST(AnyToStringTranslatorTest, RegistryLifecycleAndOverwriting) {
  // Setup initial formatting rule
  AnyToStringTranslator::Register<Point3D>([](const Point3D& pt) {
    return std::format("({},{},{})", pt.x, pt.y, pt.z);
  });

  std::any any_point = Point3D{10, -20, 30};
  EXPECT_EQ(AnyToStringTranslator::Translate(any_point), "(10,-20,30)");

  // Overwrite the registry rule for the exact same type on the fly
  AnyToStringTranslator::Register<Point3D>([](const Point3D& pt) {
    return std::format("X:{} Y:{} Z:{}", pt.x, pt.y, pt.z);
  });
  EXPECT_EQ(AnyToStringTranslator::Translate(any_point), "X:10 Y:-20 Z:30");

  // Cleanup and confirm fallback behavior drops down smoothly
  AnyToStringTranslator::Unregister<Point3D>();
  EXPECT_EQ(AnyToStringTranslator::Translate(any_point, "ERR"), "ERR");
}

TEST(AnyToStringTranslatorTest, NestedStructFormatting) {
  AnyToStringTranslator::Register<DiagnosticReport>(
      [](const DiagnosticReport& report) {
        return std::format("System:[{} Mode, Load:{:.2f}%]", report.status,
                           report.load_factor * 100.0);
      });

  std::any any_report = DiagnosticReport{"Cluster_Active", 0.7456};
  EXPECT_EQ(AnyToStringTranslator::Translate(any_report),
            "System:[Cluster_Active Mode, Load:74.56%]");

  AnyToStringTranslator::Unregister<DiagnosticReport>();
}

TEST(AnyToStringTranslatorTest, MultiThreadedConcurrenceStress) {
  // Pre-register types to prevent race conditions during translation reads
  AnyToStringTranslator::Register<Point3D>(
      [](const Point3D& pt) { return std::format("{}", pt.x); });

  constexpr int kThreadCount = 8;
  constexpr int kIterationsPerThread = 1000;
  std::vector<std::thread> workers;
  workers.reserve(kThreadCount);

  // Spawn readers and writers simultaneously to stress test the shared_mutex
  for (int t = 0; t < kThreadCount; ++t) {
    workers.emplace_back([t]() {
      std::any any_int = 42 + t;
      std::any any_pt = Point3D{t, 0, 0};

      for (int i = 0; i < kIterationsPerThread; ++i) {
        // Read operations (taking shared_lock)
        std::string res_int = AnyToStringTranslator::Translate(any_int);
        std::string res_pt = AnyToStringTranslator::Translate(any_pt);

        EXPECT_EQ(res_int, std::to_string(42 + t));
        EXPECT_EQ(res_pt, std::to_string(t));

        // Occasional write operations (taking exclusive unique_lock)
        if (i % 200 == 0) {
          AnyToStringTranslator::Register<DiagnosticReport>(
              [](const DiagnosticReport&) { return "ThreadSafeWrite"; });
        }
      }
    });
  }

  // Join all threads to ensure no memory errors or data races occur
  for (auto& thread : workers) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  // Final clean up
  AnyToStringTranslator::Unregister<Point3D>();
  AnyToStringTranslator::Unregister<DiagnosticReport>();
}
