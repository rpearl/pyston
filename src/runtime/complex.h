// Copyright (c) 2014 Dropbox, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PYSTON_RUNTIME_COMPLEX_H
#define PYSTON_RUNTIME_COMPLEX_H

#include "core/types.h"

namespace pyston {

extern "C" Box* createPureImaginary(double i);

extern "C" double mod_complex_complex(double lhs, double rhs);
extern "C" double div_complex_complex(double lhs, double rhs);
extern "C" double floordiv_complex_complex(double lhs, double rhs);
extern "C" double pow_complex_complex(double lhs, double rhs);
}

#endif