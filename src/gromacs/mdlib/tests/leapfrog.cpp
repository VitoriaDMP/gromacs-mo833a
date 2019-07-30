/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2019, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */
/*! \internal \file
 * \brief Tests for the Leap-Frog integrator
 *
 * \todo Add tests for temperature and pressure controlled integrators.
 * \todo Add PBC handling test.
 * \todo Reference values tests.
 *
 * \author Artem Zhmurov <zhmurov@gmail.com>
 * \ingroup module_mdlib
 */

#include "gmxpre.h"

#include "config.h"

#include <assert.h>

#include <cmath>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "gromacs/gpu_utils/gpu_testutils.h"
#include "gromacs/math/vec.h"
#include "gromacs/math/vectypes.h"
#include "gromacs/mdtypes/mdatom.h"
#include "gromacs/utility/stringutil.h"

#include "testutils/refdata.h"
#include "testutils/testasserts.h"

#include "leapfrogtestdata.h"
#include "leapfrogtestrunners.h"

namespace gmx
{
namespace test
{

/*! \brief The parameter space for the test.
 *
 * The test will run for all possible combinations of accessible
 * values of the:
 * 1. Number of atoms
 * 2. Timestep
 * 3-5. Velocity components
 * 6-8. Force components
 * 9. Number of steps
 */
typedef std::tuple<int, real, real, real, real, real, real, real, int> LeapFrogTestParameters;

/*! \brief Test fixture for LeapFrog integrator.
 *
 *  Creates a system of independent particles exerting constant external forces,
 *  makes several numerical integration timesteps and compares the result
 *  with analytical solution.
 *
 */
class LeapFrogTest : public ::testing::TestWithParam<LeapFrogTestParameters>
{
    public:
        //! Availiable runners (CPU and GPU versions of the Leap-Frog)
        std::unordered_map <std::string, void(*)(LeapFrogTestData *testData,
                                                 const int         numSteps)> runners_;

        LeapFrogTest()
        {
            //
            // All runners should be registered here under appropriate conditions
            //
            runners_["LeapFrogSimple"]  = integrateLeapFrogSimple;
            if (GMX_GPU == GMX_GPU_CUDA && s_hasCompatibleGpus)
            {
                runners_["LeapFrogGpu"] = integrateLeapFrogGpu;
            }
        }

        //! Store whether any compatible GPUs exist.
        static bool s_hasCompatibleGpus;
        //! Before any test is run, work out whether any compatible GPUs exist.
        static void SetUpTestCase()
        {
            s_hasCompatibleGpus = canComputeOnGpu();
        }
};

bool LeapFrogTest::s_hasCompatibleGpus = false;

TEST_P(LeapFrogTest, SimpleIntegration)
{
    // Cycle through all available runners
    for (const auto &runner : runners_)
    {
        std::string runnerName = runner.first;

        int         numAtoms; // 1. Number of atoms
        real        timestep; // 2. Timestep
        rvec        v0;       // 3. Velocity
        rvec        f0;       // 4. Force
        int         numSteps; // 5. Number of steps
        std::tie(numAtoms, timestep, v0[XX], v0[YY], v0[ZZ], f0[XX], f0[YY], f0[ZZ], numSteps) = GetParam();

        std::string testDescription = formatString("Testing %s with %d atoms for %d timestep (dt = %f, v0=(%f, %f, %f), f0=(%f, %f, %f))",
                                                   runnerName.c_str(),
                                                   numAtoms, numSteps, timestep,
                                                   v0[XX], v0[YY], v0[ZZ],
                                                   f0[XX], f0[YY], f0[ZZ]);
        SCOPED_TRACE(testDescription);

        std::unique_ptr<LeapFrogTestData> testData =
            std::make_unique<LeapFrogTestData>(numAtoms, timestep, v0, f0);

        runner.second(testData.get(), numSteps);

        real totalTime = numSteps*timestep;
        // TODO For the case of constant force, the numerical scheme is exact and
        //      the only source of errors is floating point arithmetic. Hence,
        //      the tolerance can be calculated.
        FloatingPointTolerance tolerance = absoluteTolerance(numSteps*0.000005);

        for (int i = 0; i < testData->numAtoms_; i++)
        {
            rvec xAnalytical;
            rvec vAnalytical;
            for (int d = 0; d < DIM; d++)
            {
                // Analytical solution for constant-force particle movement
                xAnalytical[d] = testData->x0_[i][d] + testData->v0_[i][d]*totalTime + 0.5*testData->f_[i][d]*totalTime*totalTime*testData->inverseMasses_[i];
                vAnalytical[d] = testData->v0_[i][d] + testData->f_[i][d]*totalTime*testData->inverseMasses_[i];

                EXPECT_REAL_EQ_TOL(xAnalytical[d], testData->xPrime_[i][d], tolerance)
                << gmx::formatString("Coordinate %d of atom %d is different from analytical solution.", d, i);

                EXPECT_REAL_EQ_TOL(vAnalytical[d], testData->v_[i][d], tolerance)
                << gmx::formatString("Velocity component %d of atom %d is different from analytical solution.", d, i);
            }
        }
    }
}

INSTANTIATE_TEST_CASE_P(WithParameters, LeapFrogTest,
                            ::testing::Combine(
                                    ::testing::Values(1, 10, 300),    // Number of atoms
                                    ::testing::Values(0.001, 0.0005), // Timestep
                                    ::testing::Values(-2.0, 0.0),     // vx
                                    ::testing::Values( 0.0, 2.0),     // vy
                                    ::testing::Values( 0.0),          // vz
                                    ::testing::Values(-1.0, 0.0),     // fx
                                    ::testing::Values( 0.0, 1.0),     // fy
                                    ::testing::Values( 2.0),          // fz
                                    ::testing::Values(1, 10)          // Number of steps
                                ));

}                                                                     // namespace test
}                                                                     // namespace gmx
