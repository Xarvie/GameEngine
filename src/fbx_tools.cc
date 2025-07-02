
#include "fbx_tools.h"

#include <algorithm>
#include <cctype>
#include <functional>

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/memory/allocator.h"
