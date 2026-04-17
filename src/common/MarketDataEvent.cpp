#include "MarketDataEvent.hpp"

const char *MarketDataEvent::sideToString(Side s) noexcept {
  switch (s) {
  case Side::Bid:
    return "Bid";
  case Side::Ask:
    return "Ask";
  case Side::None:
    return "None";
  }
  return "None";
}

const char *MarketDataEvent::actionToString(Action a) noexcept {
  switch (a) {
  case Action::Add:
    return "Add";
  case Action::Modify:
    return "Modify";
  case Action::Cancel:
    return "Cancel";
  case Action::Clear:
    return "Clear";
  case Action::Trade:
    return "Trade";
  case Action::Fill:
    return "Fill";
  case Action::None:
    return "None";
  }
  return "None";
}