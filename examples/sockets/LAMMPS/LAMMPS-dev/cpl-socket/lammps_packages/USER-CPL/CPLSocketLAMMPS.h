/*

    ________/\\\\\\\\\__/\\\\\\\\\\\\\____/\\\_____________
     _____/\\\////////__\/\\\/////////\\\_\/\\\_____________
      ___/\\\/___________\/\\\_______\/\\\_\/\\\_____________
       __/\\\_____________\/\\\\\\\\\\\\\/__\/\\\_____________
        _\/\\\_____________\/\\\/////////____\/\\\_____________
         _\//\\\____________\/\\\_____________\/\\\_____________
          __\///\\\__________\/\\\_____________\/\\\_____________
           ____\////\\\\\\\\\_\/\\\_____________\/\\\\\\\\\\\\\\\_
            _______\/////////__\///______________\///////////////__


                         C P L  -  L I B R A R Y

           Copyright (C) 2012-2015 Edward Smith & David Trevelyan

License

    This file is part of CPL-Library.

    CPL-Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CPL-Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CPL-Library.  If not, see <http://www.gnu.org/licenses/>.

Description

    "CPLSocketLAMMPS" class for interfacing with CPL-Library.

Author(s)

    David Trevelyan, Edward Smith

*/
#ifndef CPL_SOCKET_H_INCLUDED
#define CPL_SOCKET_H_INCLUDED

#include<vector>
#include<memory>

#include "mpi.h"
#include "lammps.h"
#include "fix_ave_chunk.h"
#include "region.h"

#include "cpl/cpl.h"
#include "cpl/CPL_ndArray.h"
#include "cpl/CPL_force.h"
#include "fix_cpl_force.h"

typedef CPL::ndArray<double> arrayDoub;
typedef std::shared_ptr<arrayDoub> shaPtrArrayDoub;


class CPLSocketLAMMPS
{

public:
    
    // Construct from no arguments
    CPLSocketLAMMPS() : myCoords(3), olapRegion(6), velBCRegion(6), cnstFRegion(6),
                     velBCPortion(6), cnstFPortion(6) {}
    //~CPLSocketLAMMPS();

    // Timesteps and timestep ratio
    int nsteps;
    int timestep_ratio;
    
    // Initialisation routines 
    void initComms ();
    void initMD (LAMMPS_NS::LAMMPS *lammps);

    // Data preparation and communication 
    void packVelocity(const LAMMPS_NS::LAMMPS *lammps);
    void packGran (const LAMMPS_NS::LAMMPS *lammps);
    void pack (const LAMMPS_NS::LAMMPS *lammps, int sendtype);
    void send();
    void unpackBuf(const LAMMPS_NS::LAMMPS *lammps);
    void receive();

    // Useful information for main level program
    const MPI_Comm realmCommunicator() {return realmComm;}
    const MPI_Comm cartCommunicator() {return cartComm;}
    const bool isRootProcess() {return (rankRealm == 0);}

    // Clean up MPI/CPL communicators
    void finalizeComms();

    void setupFixMDtoCFD(LAMMPS_NS::LAMMPS *lammps);
    void setupFixCFDtoMD(LAMMPS_NS::LAMMPS *lammps, std::shared_ptr<std::string> forcetype);  

    // Fix that applies the momentum constrain
    FixCPLForce* cplfix;

    //Bitwise mask coefficients
    int const VEL = 1; // 2^0, bit 0
    int const NBIN = 2;  // 2^1, bit 1
    int const FORCE = 4;  // 2^2, bit 2
    int const STRESS = 8;  // 2^3, bit 3
    int const FORCECOEFF = 16;  // 2^4, bit 4
    int const VOIDRATIO = 32;  // 2^5, bit 5

private:
    
    double VELBC_BELOW = 0.0;
    double VELBC_ABOVE = 0.0;

    // Cartesian coordinates of the processor
    std::vector<int> myCoords;

    // Communicators for use with CPL_Library
    MPI_Comm realmComm;
    MPI_Comm cartComm;

    // Rank of this processor in realm and cartComm
    int rankRealm;
    int rankCart;

    // Communication regions in the overlap
    std::vector<int> olapRegion;
    std::vector<int> velBCRegion;
    std::vector<int> cnstFRegion;

    // Portions of the regions in the local processor
    std::vector<int> velBCPortion;
    std::vector<int> cnstFPortion;

    // Number of cells in the portion for each region
    int velBCCells[3];
    int cnstFCells[3];

    // Data to be sent/received with CPL-Library
    arrayDoub sendBuf;
    arrayDoub recvBuf;

    // Cell sizes
    double dx, dy, dz;

    //Appropriate region, compute and fix    
    class LAMMPS_NS::Region *cfdbcregion, *cplforceregion;
    class LAMMPS_NS::Compute *cfdbccompute;
    class LAMMPS_NS::Fix *cfdbcfix, *cplforcefix;
    class LAMMPS_NS::Group *cplforcegroup;

    // Internal grid
    arrayDoub cfd_xg; 
    arrayDoub cfd_yg;
    arrayDoub cfd_zg;

    // Internal routines
    void getCellTopology();
    void allocateBuffers(const LAMMPS_NS::LAMMPS *lammps, int sendtype);

    //Size of different possible passing variables
    int const VELSIZE = 3;  // 3 velocity components
    int const NBINSIZE = 1;  // 1 Number of molecules per bin
    int const FORCESIZE = 3;  // 3 Force components
    int const STRESSSIZE = 9;  // 9 Nine stress components
    int const FORCECOEFFSIZE = 1;  // 1 Sum of force coefficients
    int const VOIDRATIOSIZE = 1;  // 1 Void ratio

};

#endif // CPL_SOCKET_H_INCLUDED
