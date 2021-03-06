/**
 *  @file   larpandoracontent/LArControlFlow/NeutrinoIdTool.cc
 *
 *  @brief  Implementation of the neutrino id tool class.
 *
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "larpandoracontent/LArControlFlow/NeutrinoIdTool.h"

using namespace pandora;

namespace lar_content
{

NeutrinoIdTool::NeutrinoIdTool() :
    m_selectAllNeutrinos(true),
    m_selectOnlyFirstSliceNeutrinos(false)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

void NeutrinoIdTool::SelectOutputPfos(const SliceHypotheses &nuSliceHypotheses, const SliceHypotheses &crSliceHypotheses, PfoList &selectedPfos)
{
    if (nuSliceHypotheses.size() != crSliceHypotheses.size())
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);

    for (unsigned int sliceIndex = 0, nSlices = nuSliceHypotheses.size(); sliceIndex < nSlices; ++sliceIndex)
    {
        const PfoList &sliceOutput((m_selectAllNeutrinos || (m_selectOnlyFirstSliceNeutrinos && (0 == sliceIndex))) ?
            nuSliceHypotheses.at(sliceIndex) : crSliceHypotheses.at(sliceIndex));

        selectedPfos.insert(selectedPfos.end(), sliceOutput.begin(), sliceOutput.end());
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode NeutrinoIdTool::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "SelectAllNeutrinos", m_selectAllNeutrinos));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "SelectOnlyFirstSliceNeutrinos", m_selectOnlyFirstSliceNeutrinos));

    if (m_selectAllNeutrinos == m_selectOnlyFirstSliceNeutrinos)
    {
        std::cout << "NeutrinoIdTool::ReadSettings - exactly one of SelectAllNeutrinos and SelectOnlyFirstSliceNeutrinos must be true" << std::endl;
        return STATUS_CODE_INVALID_PARAMETER;
    }

    return STATUS_CODE_SUCCESS;
}

} // namespace lar_content
