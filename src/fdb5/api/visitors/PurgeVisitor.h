/*
 * (C) Copyright 2018- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

/// @author Simon Smart
/// @date   November 2018

#ifndef fdb5_api_visitor_PurgeVisitor_H
#define fdb5_api_visitor_PurgeVisitor_H

#include "fdb5/api/visitors/QueryVisitor.h"
#include "fdb5/api/helpers/PurgeIterator.h"

#include "eckit/filesystem/PathName.h"


namespace fdb5 {
namespace api {
namespace visitor {

/// @note Helper classes for LocalFDB

//----------------------------------------------------------------------------------------------------------------------

class PurgeVisitor : public QueryVisitor<PurgeElement> {
public:

    PurgeVisitor(eckit::Queue<PurgeElement>& queue, bool doit);

    void visitDatabase(const DB& db) override;
    void visitIndex(const Index& index) override;
    void databaseComplete(const DB& db) override;
    void visitDatum(const Field&, const Key&) override { NOTIMP; }

private: // members

    bool doit_;
};

//----------------------------------------------------------------------------------------------------------------------

} // namespace visitor
} // namespace api
} // namespace fdb5

#endif
