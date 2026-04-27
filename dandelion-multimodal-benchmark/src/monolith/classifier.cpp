#include "classifier.h"
#include <algorithm>
#include <cctype>

// Returns true if `text` contains any of the given keywords.
static bool contains_any(const std::string& text,
                          std::initializer_list<const char*> keywords) {
    for (const char* kw : keywords)
        if (text.find(kw) != std::string::npos)
            return true;
    return false;
}

std::string classify_intent(const std::string& normalized_text) {
    const std::string& t = normalized_text; // already lowercased

    // Order matters: more specific patterns first.
    if (contains_any(t, {"cancel", "cancellation", "remove my booking",
                          "delete reservation", "no longer coming"}))
        return "cancel_reservation";

    if (contains_any(t, {"change", "modify", "update", "reschedule",
                          "move my", "move the", "add more people",
                          "add two", "add three", "add four",
                          "instead of", "switch from"}))
        return "modify_reservation";

    if (contains_any(t, {"book", "reserve", "reservation", "table for",
                          "seats for", "booking", "make a reservation",
                          "get a table", "place for"}))
        return "book_reservation";

    if (contains_any(t, {"menu", "food", "dish", "dishes", "eat",
                          "vegan", "vegetarian", "gluten", "dairy",
                          "allergen", "specials", "starter", "dessert",
                          "appetizer", "cuisine", "chef", "ingredients",
                          "kids menu", "children"}))
        return "ask_about_menu";

    return "unknown";
}
