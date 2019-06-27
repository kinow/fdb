/*
 * (C) Copyright 1996- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

/// @author Simon Smart
/// @date   Mar 2018

#ifndef fdb5_api_SelectFDB_H
#define fdb5_api_SelectFDB_H

#include <vector>
#include <map>
#include <string>

#include "eckit/utils/Regex.h"

#include "fdb5/api/FDBFactory.h"
#include "fdb5/api/FDB.h"

namespace fdb5 {

//----------------------------------------------------------------------------------------------------------------------

class SelectFDB : public FDBBase {

private: // types

    using SelectMap = std::map<std::string, eckit::Regex>;

public: // methods

    SelectFDB(const Config& config, const std::string& name);

    ~SelectFDB() override;

    void archive(const Key& key, const void* data, size_t length) override;

    eckit::DataHandle* retrieve(const metkit::MarsRequest& request) override;

    ListIterator list(const FDBToolRequest& request) override;

    DumpIterator dump(const FDBToolRequest& request, bool simple) override;

    WhereIterator where(const FDBToolRequest& request) override;

    WipeIterator wipe(const FDBToolRequest& request, bool doit, bool porcelain) override;

    PurgeIterator purge(const FDBToolRequest& request, bool doit, bool porcelain) override;

    StatsIterator stats(const FDBToolRequest& request) override;

    void flush() override;

private: // methods

    void print(std::ostream& s) const override;

    bool matches(const Key& key, const SelectMap& select, bool requireMissing) const;
    bool matches(const metkit::MarsRequest& request, const SelectMap& select, bool requireMissing) const;

    template <typename QueryFN>
    auto queryInternal(const FDBToolRequest& request, const QueryFN& fn) -> decltype(fn(*(FDB*)(nullptr), request));

private: // members

    std::vector<std::pair<SelectMap, FDB>> subFdbs_;
};

//----------------------------------------------------------------------------------------------------------------------

} // namespace fdb5

#endif // fdb5_api_SelectFDB_H
