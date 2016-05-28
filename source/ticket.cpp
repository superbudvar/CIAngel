#include "ticket.h"

extern const u16 tmd_top;

void CreateTicket(std::string titleId, std::string encTitleKey, char* titleVersion, std::string outputFullPath)
{
    std::ofstream ofs;

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    ofs.write(tikTemp, TICKET_SIZE);
    ofs.close();

    ofs.open(outputFullPath, std::ofstream::out | std::ofstream::in | std::ofstream::binary);

    //write version
    ofs.seekp(tmd_top+0xA6, std::ios::beg);
    ofs.write(titleVersion, 0x2);

    //write title id
    char* nTitleID = parse_string(titleId);
    ofs.seekp(tmd_top+0x9C, std::ios::beg);
    ofs.write(nTitleID, 0x8);
    free(nTitleID);

    //write key
    char* nTitleKey = parse_string(encTitleKey);
    ofs.seekp(tmd_top+0x7F, std::ios::beg);
    ofs.write(nTitleKey, 0x10);
    free(nTitleKey);

    ofs.close();
}

void InstallTicket(std::string FullPath, std::string TitleId)
{
    Handle hTik;
    u32 writtenbyte;
    std::string curr = get_file_contents(FullPath.c_str());

    // Remove the ticket incase there was a bad one previously installed
    char* nTitleId = parse_string(TitleId);
    u64 titleId = u8_to_u64((u8*)nTitleId, BIG_ENDIAN);
    free (nTitleId);
    AM_DeleteTicket(titleId);

    // Install new ticket
    AM_InstallTicketBegin(&hTik);
    FSFILE_Write(hTik, &writtenbyte, 0, curr.c_str(), 0x100000, 0);
    AM_InstallTicketFinish(hTik);
    printf("Ticket Installed.");
    //delete temp ticket, ticket folder still exists... ugly. later stream directly to the handle
    remove(FullPath.c_str());
}
