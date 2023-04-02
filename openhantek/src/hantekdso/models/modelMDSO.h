// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "dsomodel.h"

class HantekDsoControl;
using namespace Hantek;


struct ModelMDSO : public DSOModel {
    static const int ID = 0x1D50;
    ModelMDSO();
    void applyRequirements( HantekDsoControl *dsoControl ) const override;
};
