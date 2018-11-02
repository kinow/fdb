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

#ifndef fdb5_api_FDBFactory_H
#define fdb5_api_FDBFactory_H

#include "fdb5/config/Config.h"
#include "fdb5/api/FDBStats.h"

#include "eckit/utils/Regex.h"
#include "eckit/memory/NonCopyable.h"
#include "fdb5/api/FDBListObject.h"

#include <memory>

class MarsRequest;


namespace fdb5 {

class Key;
class FDBToolRequest;

//----------------------------------------------------------------------------------------------------------------------

/// The base class that FDB implementations are derived from

class FDBBase : private eckit::NonCopyable {

public: // methods

    FDBBase(const Config& config, const std::string& name);
    virtual ~FDBBase();

    // -------------- Primary API functions ----------------------------

    virtual void archive(const Key& key, const void* data, size_t length) = 0;

    virtual void flush() = 0;

    virtual eckit::DataHandle* retrieve(const MarsRequest& request) = 0;

    virtual FDBListObject list(const FDBToolRequest& request) = 0;

    // -------------- API management ----------------------------

    /// ID used for hashing in the Rendezvous hash. Should be unique amongst those used
    /// within a DistFDB (i.e. within one Rendezvous hash).
    virtual std::string id() const = 0;

    virtual FDBStats stats() const;

    const std::string& name() const;

    bool writable();
    bool visitable();
    bool disabled();

    void disable();

private: // methods

    virtual void print(std::ostream& s) const = 0;

    friend std::ostream& operator<<(std::ostream& s, const FDBBase& f) {
        f.print(s);
        return s;
    }

protected: // members

    const std::string name_;

    Config config_;

    bool writable_;
    bool visitable_;
    bool disabled_;
};

//----------------------------------------------------------------------------------------------------------------------

class FDBBuilderBase;

class FDBFactory {
public:

    static FDBFactory& instance();

    void add(const std::string& name, const FDBBuilderBase*);

    std::unique_ptr<FDBBase> build(const Config& config);

private:

    FDBFactory() {} ///< private constructor only used by singleton

    eckit::Mutex mutex_;

    std::map<std::string, const FDBBuilderBase*> registry_;

};


class FDBBuilderBase {
public: // methods

    virtual std::unique_ptr<FDBBase> make(const Config& config) const = 0;

protected: // methods

    FDBBuilderBase(const std::string& name);

    virtual ~FDBBuilderBase();

protected: // members

    std::string name_;
};



template <typename T>
class FDBBuilder : public FDBBuilderBase {

    static_assert(std::is_base_of<FDBBase, T>::value, "FDB Factorys can only build implementations of the FDB interface");

public: // methods

    FDBBuilder(const std::string& name) : FDBBuilderBase(name) {}

private: // methods

    virtual std::unique_ptr<FDBBase> make(const Config& config) const {
        return std::unique_ptr<T>(new T(config, name_));
    }
};

//----------------------------------------------------------------------------------------------------------------------

} // namespace fdb5

#endif // fdb5_api_FDBFactory_H
