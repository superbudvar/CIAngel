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
    ofs.seekp(tmd_top+0x9C, std::ios::beg);
    ofs.write(parse_string(titleId), 0x8);

    //write key
    ofs.seekp(tmd_top+0x7F, std::ios::beg);
    ofs.write(parse_string(encTitleKey), 0x10);

    ofs.close();
}

void InstallTicket(std::string FullPath)
{
    Handle hTik;
    u32 writtenbyte;
    AM_InstallTicketBegin(&hTik);
    std::string curr = get_file_contents(FullPath.c_str());
    FSFILE_Write(hTik, &writtenbyte, 0, curr.c_str(), 0x100000, 0);
    AM_InstallTicketFinish(hTik);
    printf("Ticket Installed.");
    //delete temp ticket, ticket folder still exists... ugly. later stream directly to the handle
    remove(FullPath.c_str());
}
