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

#ifndef PYSTON_ANALYSIS_SCOPINGANALYSIS_H
#define PYSTON_ANALYSIS_SCOPINGANALYSIS_H

#include "core/common.h"

namespace pyston {

class AST;
class AST_Module;

class ScopeInfo {
public:
    ScopeInfo() : isGeneratorValue(false) {}
    virtual ~ScopeInfo() {}
    virtual ScopeInfo* getParent() = 0;

    virtual bool createsClosure() = 0;
    virtual bool takesClosure() = 0;
    virtual bool passesThroughClosure() = 0;

    virtual bool takesGenerator() { return isGeneratorValue; }
    virtual void setTakesGenerator(bool b = true) { isGeneratorValue = b; }

    virtual bool refersToGlobal(const std::string& name) = 0;
    virtual bool refersToClosure(const std::string name) = 0;
    virtual bool saveInClosure(const std::string name) = 0;

    // Get the names set within a classdef that should be forwarded on to
    // the metaclass constructor.
    // An error to call this on a non-classdef node.
    virtual const std::unordered_set<std::string>& getClassDefLocalNames() = 0;

protected:
    bool isGeneratorValue;
};

class ScopingAnalysis {
public:
    struct ScopeNameUsage;
    typedef std::unordered_map<AST*, ScopeNameUsage*> NameUsageMap;

private:
    std::unordered_map<AST*, ScopeInfo*> scopes;
    AST_Module* parent_module;

    std::unordered_map<AST*, AST*> scope_replacements;

    ScopeInfo* analyzeSubtree(AST* node);
    void processNameUsages(NameUsageMap* usages);

public:
    // The scope-analysis is done before any CFG-ization is done,
    // but many of the queries will be done post-CFG-ization.
    // The CFG process can replace scope AST nodes with others (ex:
    // generator expressions with generator functions), so we need to
    // have a way of mapping the original analysis with the new queries.
    // This is a hook for the CFG process to register when it has replaced
    // a scope-node with a different node.
    void registerScopeReplacement(AST* original_node, AST* new_node);

    ScopingAnalysis(AST_Module* m);
    ScopeInfo* getScopeInfoForNode(AST* node);
};

ScopingAnalysis* runScopingAnalysis(AST_Module* m);

bool containsYield(AST* ast);
}

#endif
