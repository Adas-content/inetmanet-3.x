//
// Copyright (C) 2013 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/physicallayer/scalar/ScalarReceiver.h"
#include "inet/physicallayer/scalar/ScalarReception.h"
#include "inet/physicallayer/scalar/ScalarNoise.h"
#include "inet/physicallayer/scalar/ScalarSNIR.h"
#include "inet/physicallayer/common/BandListening.h"

namespace inet {

namespace physicallayer {

Define_Module(ScalarReceiver);

ScalarReceiver::ScalarReceiver() :
    FlatReceiverBase()
{
}

void ScalarReceiver::printToStream(std::ostream& stream) const
{
    stream << "ScalarReceiver, ";
    FlatReceiverBase::printToStream(stream);
}

const INoise *ScalarReceiver::computeNoise(const IListening *listening, const IInterference *interference) const
{
    const BandListening *bandListening = check_and_cast<const BandListening *>(listening);
    Hz carrierFrequency = bandListening->getCarrierFrequency();
    Hz bandwidth = bandListening->getBandwidth();
    simtime_t noiseStartTime = SimTime::getMaxTime();
    simtime_t noiseEndTime = 0;
    std::map<simtime_t, W> *powerChanges = new std::map<simtime_t, W>();
    const std::vector<const IReception *> *interferingReceptions = interference->getInterferingReceptions();
    for (std::vector<const IReception *>::const_iterator it = interferingReceptions->begin(); it != interferingReceptions->end(); it++) {
        const ScalarReception *reception = check_and_cast<const ScalarReception *>(*it);
        if (carrierFrequency == reception->getCarrierFrequency() && bandwidth == reception->getBandwidth()) {
            W power = reception->getPower();
            simtime_t startTime = reception->getStartTime();
            simtime_t endTime = reception->getEndTime();
            if (startTime < noiseStartTime)
                noiseStartTime = startTime;
            if (endTime > noiseEndTime)
                noiseEndTime = endTime;
            std::map<simtime_t, W>::iterator itStartTime = powerChanges->find(startTime);
            if (itStartTime != powerChanges->end())
                itStartTime->second += power;
            else
                powerChanges->insert(std::pair<simtime_t, W>(startTime, power));
            std::map<simtime_t, W>::iterator itEndTime = powerChanges->find(endTime);
            if (itEndTime != powerChanges->end())
                itEndTime->second -= power;
            else
                powerChanges->insert(std::pair<simtime_t, W>(endTime, -power));
        }
        else if (areOverlappingBands(carrierFrequency, bandwidth, reception->getCarrierFrequency(), reception->getBandwidth()))
            throw cRuntimeError("Overlapping bands are not supported");
    }
    const ScalarNoise *scalarBackgroundNoise = dynamic_cast<const ScalarNoise *>(interference->getBackgroundNoise());
    if (scalarBackgroundNoise) {
        if (carrierFrequency == scalarBackgroundNoise->getCarrierFrequency() && bandwidth == scalarBackgroundNoise->getBandwidth()) {
            const std::map<simtime_t, W> *backgroundNoisePowerChanges = scalarBackgroundNoise->getPowerChanges();
            for (std::map<simtime_t, W>::const_iterator it = backgroundNoisePowerChanges->begin(); it != backgroundNoisePowerChanges->end(); it++) {
                std::map<simtime_t, W>::iterator jt = powerChanges->find(it->first);
                if (jt != powerChanges->end())
                    jt->second += it->second;
                else
                    powerChanges->insert(std::pair<simtime_t, W>(it->first, it->second));
            }
        }
        else if (areOverlappingBands(carrierFrequency, bandwidth, scalarBackgroundNoise->getCarrierFrequency(), scalarBackgroundNoise->getBandwidth()))
            throw cRuntimeError("Overlapping bands are not supported");
    }
    EV_DEBUG << "Noise power begin " << endl;
    W noise = W(0);
    for (std::map<simtime_t, W>::const_iterator it = powerChanges->begin(); it != powerChanges->end(); it++) {
        noise += it->second;
        EV << "Noise at " << it->first << " = " << noise << endl;
    }
    EV_DEBUG << "Noise power end" << endl;
    return new ScalarNoise(noiseStartTime, noiseEndTime, carrierFrequency, bandwidth, powerChanges);
}

const ISNIR *ScalarReceiver::computeSNIR(const IReception *reception, const INoise *noise) const
{
    const ScalarReception *scalarReception = check_and_cast<const ScalarReception *>(reception);
    const ScalarNoise *scalarNoise = check_and_cast<const ScalarNoise *>(noise);
    return new ScalarSNIR(scalarReception, scalarNoise);
}

} // namespace physicallayer

} // namespace inet

