#include <gtest/gtest.h>
#include <ftxmodel/any_to_string.hpp>

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
