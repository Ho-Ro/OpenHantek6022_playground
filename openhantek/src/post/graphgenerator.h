// SPDX-License-Identifier: GPL-2.0+

#pragma once

#include <deque>

#include <QObject>
#include <QVector3D>

#include "hantekdso/enums.h"
#include "hantekprotocol/types.h"
#include "processor.h"

struct DsoSettingsScope;
struct DsoSettingsView;
class PPresult;
namespace Dso {
struct ControlSpecification;
}

/// \brief Generates ready to be used vertex arrays
class GraphGenerator : public QObject, public Processor {
    Q_OBJECT

  public:
    GraphGenerator( const DsoSettingsScope *scope, const DsoSettingsView *view );

  private:
    void generateGraphsTYvoltage( PPresult *result );
    void generateGraphsTYspectrum( PPresult *result );
    void generateGraphsXY( PPresult *result );

    bool ready = false;
    const DsoSettingsScope *scope;
    const DsoSettingsView *view;

    // Processor interface
    void process( PPresult *data ) override;
};
