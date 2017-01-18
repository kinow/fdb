/*
 * (C) Copyright 1996-2017 ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

/// @file   NotifyWind.h
/// @author Baudouin Raoult
/// @author Tiago Quintino
/// @date   Mar 2016

#ifndef fdb5_NotifyWind_H
#define fdb5_NotifyWind_H



namespace fdb5 {


//----------------------------------------------------------------------------------------------------------------------

class NotifyWind {

public: // methods

    NotifyWind();

    virtual ~NotifyWind();

    virtual void notifyWind() const = 0;
};

//----------------------------------------------------------------------------------------------------------------------

} // namespace fdb5

#endif
