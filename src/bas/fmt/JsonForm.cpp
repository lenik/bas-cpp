#include "JsonForm.hpp"

const JsonFormOptions JsonFormOptions::DEFAULT = {
    false,
    false,
    2,
    -1,
    true,
};

const JsonFormOptions JsonFormOptions::COMPACT = {
    false,
    true,
    0,
    -1,
    true,
};
