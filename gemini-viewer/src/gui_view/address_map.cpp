#include "address_map.h"

#include <QFile>
#include <QByteArray>
#include <QMessageBox>
#include <QDebug>

Address_map::Address_map(QString filename)
    : m_filename(filename)
    , is_loaded(false)
{
}
Address_map::~Address_map()
{
    qDebug() << "Address_map::~Address_map() Goodbye";
}

// Load configuration data from address map file
bool Address_map::load()
{
    qDebug() << "Address_map::load()  Loading FPGA address map" <<  m_filename;
    QFile cfg_file(m_filename);
    if (!cfg_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug().noquote() << "Address_map::load() Cant open " << m_filename;
        return false;
    }

    while (!cfg_file.atEnd())
    {
        QByteArray line = cfg_file.readLine();

        // Strip comments
        int idx = line.indexOf('#');
        if(idx >= 0)
            line.truncate(idx);

        bool err = process_line(line);
        if(err)
        {
            qDebug().nospace().noquote() << "ERROR: " << line;
            cfg_file.close();
            return false;
        }
    }

    cfg_file.close();

    // Put all the axi slave port data into address-sorted order
    std::sort(m_axi_ports.begin(), m_axi_ports.end());
    is_loaded = true;

    // Debug:: dump the sorted address map into the log
    //dump();
    check_holes();
    return true;
}

bool Address_map::isLoadedOk()
{
    return is_loaded;
}

// Process a line from the address map. Format should be:
// "      BitField   0x00000800 b[31:0] RO dhcp dhcp dhcp_server"
// ie       type       gem_addr  bit    mode Periph_name Port_name Field_name

// "      DistrRAM   0x00000420 len=32 RO sfp sfp results "
// "      FIFO       0x00000420 len=32 RO sfp sfp results "
// "      BlockRAM   0x00000420 len=32 RO sfp sfp results "
// ie       type       gem_addr  nwords  mode Periph_name Port_name Field_name
bool Address_map::process_line(QString line)
{
    if(line.length() == 0)
        return false;

    QList<QString> tokens = tokenise(line);
    if(tokens.length() == 0)
        return false;
    if (tokens.length() != 7)
    {
        QMessageBox msg;
        msg.setText("Error: In config file, expected 7 fields, got "
                    + QString("%1").arg(tokens.length()));
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("FPGA config file error");
        msg.exec();
        qDebug().noquote().nospace() << "ERROR: Too many tokens ("
                 << tokens.length()
                 << ") on line in config file '" << m_filename << "'.";
        return true;
    }

    // First token is type of data: BitField, DistrRAM, BlockRAM, FIFO
    QString type_of_data = tokens.front();
    tokens.pop_front();
    bool err;
    if(type_of_data == "BitField")
        err = make_bitfield(tokens);
    else if(type_of_data == "DistrRAM")
        err = make_distram(tokens);
    else if(type_of_data == "BlockRAM")
        err = make_blockram(tokens);
    else if(type_of_data == "FIFO")
        err = make_fifo(tokens);
    else
    {
        QMessageBox msg;
        msg.setText("Error: Unknown '" + type_of_data
                    + "' in config file.");
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("FPGA config file error");
        msg.exec();
        qDebug().noquote().nospace() << "ERROR: unknown '"
                 << type_of_data
                 << "' in config file '" << m_filename << "'.";
        return true;
    }
    return (err);
 }


bool Address_map::make_bitfield(QList<QString> &tokens)
{
    // Second token is address base (usually in hex 0xNNNNNN format)
    bool addr_ok;
    uint32_t addr = tokens.front().toInt(&addr_ok, 0);
    if(!addr_ok)
        qDebug() << "Cant convert address" << tokens.front();
    tokens.pop_front();
    // Third field describes where the bitfield is in the word, ie 'b[nn:mm]'
    bool len_ok = false;
    uint32_t bit_base = 0;
    uint32_t bit_width = 32;
    QString & t = tokens.front();
    int i = t.indexOf(':');
    if(i == -1)
    {
        qDebug() << "Can't find ':' in bitfield line";
        return true;
    }
    int top = t.mid(2, i-2).toInt(&len_ok, 0);
    if(len_ok)
    {
        int bot = t.mid(i+1, t.length()-2-i).toInt(&len_ok, 0);
        if(len_ok)
        {
            bit_base = bot;
            bit_width = (top+1)-bot;
        }
        else
            qDebug() << "Cant convert bottom address"
                     << t.mid(i+1, t.length()-2-i);
    }
    else
        qDebug() << "Cant convert top address" << t.mid(2,i-2);
    tokens.pop_front();

    if( (!addr_ok) || (!len_ok))
    {
        QMessageBox msg;
        msg.setText("Error: Bad BitField line in in config file '"
                    + m_filename + "'.");
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("FPGA config file error");
        msg.exec();
        return true;
    }

    Args_field_t a;
    a.base_addr = addr;
    a.f_type = Field_t::BitField;
    a.n_regs = 1;
    a.bit_base = bit_base;
    a.bit_width = bit_width;
    if( tokens.front() == "RW")
        a.rw = Access_t::RW;
    else if (tokens.front() == "RO")
        a.rw = Access_t::RO;
    else
        a.rw = Access_t::WO;
    tokens.pop_front();
    QString peripheral_name = tokens.front();
    tokens.pop_front();
    QString port_name = tokens.front();
    tokens.pop_front();
    a.field_name = tokens.front();
    save_field(a, peripheral_name, port_name);

    return false;
}

bool Address_map::make_distram(QList<QString> &tokens)
{
    // Second token is address base (usually in hex 0xNNNNNN format)
    bool addr_ok;
    uint32_t addr = tokens.front().toInt(&addr_ok, 0);
    if(!addr_ok)
        qDebug() << "Cant convert DistrRAM address" << tokens.front();
    tokens.pop_front();

    uint32_t bit_base = 0;
    uint32_t bit_width = 32;
    // Third field describes where the bitfield is in the word, eg 'b[nn:mm]'
    uint32_t field_len;
    bool len_ok = false;
    QString & t = tokens.front();
    int i = t.indexOf('=');
    if(i == -1)
    {
        qDebug() << "Cant find '=' in length field" << t;
        return true;
    }
    field_len = t.right(t.length()-1-i).toInt(&len_ok, 0);
    if(! len_ok)
        qDebug() << "Cant convert length field" << t.right(t.length()-2-i);

    if(!addr_ok || !len_ok)
    {
        QMessageBox msg;
        msg.setText("Error: Bad BitField line in in config file '"
                    + m_filename + "'.");
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("FPGA config file error");
        msg.exec();
        return true;
    }

    Args_field_t a;
    a.base_addr = addr;
    a.f_type = Field_t::DistRAM;
    a.n_regs = field_len;
    a.bit_base = bit_base;
    a.bit_width = bit_width;
    tokens.pop_front();
    if( tokens.front() == "RW")
        a.rw = Access_t::RW;
    else if (tokens.front() == "RO")
        a.rw = Access_t::RO;
    else
        a.rw = Access_t::WO;
    tokens.pop_front();
    QString peripheral_name = tokens.front();
    tokens.pop_front();
    QString port_name = tokens.front();
    tokens.pop_front();
    a.field_name = tokens.front();
    save_field(a, peripheral_name, port_name);

    return false;
}
bool Address_map::make_blockram(QList<QString> &tokens)
{
    // Second token is address base (usually in hex 0xNNNNNN format)
    bool addr_ok;
    uint32_t addr = tokens.front().toInt(&addr_ok, 0);
    if(!addr_ok)
        qDebug() << "Cant convert BlockRAM address" << tokens.front();
    tokens.pop_front();

    uint32_t bit_base = 0;
    uint32_t bit_width = 32;
    // Third field describes where the bitfield is in the word, eg 'b[nn:mm]'
    uint32_t field_len;
    bool len_ok = false;
    QString & t = tokens.front();
    int i = t.indexOf('=');
    if(i == -1)
        return true;
    field_len = t.right(t.length()-1-i).toInt(&len_ok, 0);
    if(! len_ok)
        qDebug() << "Cant convert length field" << t.right(t.length()-2-i);


    if(!addr_ok || !len_ok)
    {
        QMessageBox msg;
        msg.setText("Error: Bad BitField line in in config file '"
                    + m_filename + "'.");
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("FPGA config file error");
        msg.exec();
        return true;
    }

    Args_field_t a;
    a.base_addr = addr;
    a.f_type = Field_t::BlockRAM;
    a.n_regs = field_len;
    a.bit_base = bit_base;
    a.bit_width = bit_width;
    tokens.pop_front();
    if( tokens.front() == "RW")
        a.rw = Access_t::RW;
    else if (tokens.front() == "RO")
        a.rw = Access_t::RO;
    else
        a.rw = Access_t::WO;
    tokens.pop_front();
    QString peripheral_name = tokens.front();
    tokens.pop_front();
    QString port_name = tokens.front();
    tokens.pop_front();
    a.field_name = tokens.front();
    save_field(a, peripheral_name, port_name);

    return false;
}
bool Address_map::make_fifo(QList<QString> &tokens)
{
    // Second token is address base (usually in hex 0xNNNNNN format)
    bool addr_ok;
    uint32_t addr = tokens.front().toInt(&addr_ok, 0);
    if(!addr_ok)
        qDebug() << "Cant convert FIFO address" << tokens.front();
    tokens.pop_front();

    uint32_t bit_base = 0;
    uint32_t bit_width = 32;
    // Third field describes where the bitfield is in the word, eg 'b[nn:mm]'
    uint32_t field_len;
    bool len_ok = false;
    QString & t = tokens.front();
    int i = t.indexOf('=');
    if(i == -1)
        return true;
    field_len = t.right(t.length()-1-i).toInt(&len_ok, 0);
    if(! len_ok)
        qDebug() << "Cant convert length field" << t.right(t.length()-2-i);


    if(!addr_ok || !len_ok)
    {
        QMessageBox msg;
        msg.setText("Error: Bad BitField line in in config file '"
                    + m_filename + "'.");
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("FPGA config file error");
        msg.exec();
        return true;
    }

    Args_field_t a;
    a.base_addr = addr;
    a.f_type = Field_t::FIFO;
    a.n_regs = field_len;
    a.bit_base = bit_base;
    a.bit_width = bit_width;
    tokens.pop_front();
    if( tokens.front() == "RW")
        a.rw = Access_t::RW;
    else if (tokens.front() == "RO")
        a.rw = Access_t::RO;
    else
        a.rw = Access_t::WO;
    tokens.pop_front();
    QString peripheral_name = tokens.front();
    tokens.pop_front();
    QString port_name = tokens.front();
    tokens.pop_front();
    a.field_name = tokens.front();
    save_field(a, peripheral_name, port_name);

    return false;
}

// Pull white-space delimited tokens out of a string and return a list
QList<QString> Address_map::tokenise(QString line)
{
    QList<QString> token_list;
    bool inToken = false;
    int token_start = 0;
    for (int i=0; i<line.length(); i++)
    {
        if(inToken)
        {
            if (line[i].isSpace())
            {
                token_list.append (line.mid(token_start,i-token_start));
                inToken = false;
            }
        }
        else
        {
            if ( ! line[i].isSpace())
            {
                token_start = i;
                inToken = true;
            }
        }
    }
    if(inToken)
    {
        token_list.append(line.right(line.length() - token_start));
    }
    return token_list;
}

void Address_map::dump()
{
    qDebug() << "Total of "<< m_axi_ports.size() << "AXI Ports";
    for(auto port: m_axi_ports)
    {
        qDebug() << port.peripheral_name << port.port_name << "("
                 << port.n_regs << "words)";
        for(auto field: port.fields)
        {
            QString txt;
            if(field.f_type == Field_t::BitField)
                txt = QString("Bitfield");
            else if(field.f_type == Field_t::DistRAM)
                txt = QString("DistrRAM");
            else if(field.f_type == Field_t::BlockRAM)
                txt = QString("BlockRAM");
            else if(field.f_type == Field_t::FIFO)
                txt = QString("FIFO    ");
            else
                txt = QString("Unknown ");

            txt += QString(" 0x%1").arg(field.base_addr,8,16,QLatin1Char('0'));
            txt += QString(" b[%1").arg(field.bit_base+field.bit_width-1);
            txt += QString(":%1]").arg(field.bit_base);
            txt += QString(" len=%1").arg(field.n_regs);

            switch(field.rw)
            {
                case Access_t::RW:
                    txt += " RW";
                    break;
                case Access_t::RO:
                    txt += " RO";
                    break;
                case Access_t::WO:
                    txt += " WO";
                    break;
                default:
                    txt += " Unk";
                    break;
            }

            txt += " ";
            txt += port.peripheral_name;
            txt += ".";
            txt += port.port_name;
            txt += ".";
            txt += field.field_name;

            qDebug().noquote().nospace() << txt;
        }
    }
}

void Address_map::check_holes()
{
    int n_holes = 0;
    qDebug() << "Total of "<< m_axi_ports.size() << "Axi Ports:";
    for(auto port: m_axi_ports)
    {
        bool first = true;
        uint32_t next_addr = 0;
        uint32_t last_addr = 0;
        for(auto field: port.fields)
        {
            if(first)
            {
                first = false;
            }
            else
            {
                if((field.base_addr != next_addr)
                        && (field.base_addr != last_addr))
                {
                    QString txt = QString("Hole before 0x%1").arg(field.base_addr,8,16,QLatin1Char('0'));
                    qDebug().noquote().nospace() << txt;
                    n_holes++;
                }
            }
            next_addr = field.base_addr + field.n_regs;
            last_addr = field.base_addr;
        }
    }
    if(n_holes == 0)
        qDebug() << "No holes in any slave address map";
    else
        if(n_holes == 1)
            qDebug() << "One address map hole found";
        else
            qDebug() << n_holes << "address map holes found";
}

void Address_map::save_field(Args_field_t f
                              , QString &peripheral_name, QString &port_name)
{
    for(auto it=m_axi_ports.begin(); it!=m_axi_ports.end(); it++)
    {
        // Add field to existing port if it exists
        if((it->peripheral_name == peripheral_name)
                 && (it->port_name == port_name))
        {
            if(f.base_addr < it->base_addr)
            {
                uint32_t n_regs = it->n_regs;
                n_regs += (it->base_addr - f.base_addr);
                it->base_addr = f.base_addr;
                it->n_regs = n_regs;
            }
            else
            {
                uint32_t last_reg_in_field = f.base_addr + f.n_regs - 1;
                uint32_t last_reg_in_port = it->base_addr + it->n_regs - 1;
                if(last_reg_in_field > last_reg_in_port)
                {
                    it->n_regs += (last_reg_in_field - last_reg_in_port);
                }
            }
            // Debug: Show each field as its found in the ".ccfg" config file
            //qDebug() << "Address_map::save_field " << f.field_name << "(periph="
            //         << peripheral_name << port_name << ")";
            it->fields.push_back(f);
            std::sort(it->fields.begin(), it->fields.end());
            return;
         }
    }

    // Create new port and add field to it
    qDebug() << "Address_map::save_field  New Peripheral"
             << peripheral_name << port_name << ", field " << f.field_name ;
    Axi_port_t axi_port;
    axi_port.base_addr = f.base_addr;
    axi_port.n_regs = f.n_regs;
    axi_port.peripheral_name = peripheral_name;
    axi_port.port_name = port_name;
    axi_port.fields.push_back(f);
    m_axi_ports.push_back(axi_port);
}

bool Args_field_t::operator< (const Args_field_t rhs) const
{
    if(base_addr < rhs.base_addr)
        return true;
    if(base_addr == rhs.base_addr)
        return bit_base < rhs.bit_base;
    return false;
}

bool Axi_port_t::operator< (const Axi_port_t rhs) const
{
    return(base_addr < rhs.base_addr);
}

int Address_map::get_num_ports()
{
    return m_axi_ports.size();
}

Axi_port_t * Address_map::get_port(int i)
{
    if ((i<0) || (i>=m_axi_ports.size()))
        return nullptr;
    return & m_axi_ports[i];
}

QVector<Axi_port_t> & Address_map::all_ports()
{
    return m_axi_ports;
}

// Find base address of the register field named "field" which is part of the
// register group named "group" and in the peripheral named "peripheral
uint32_t Address_map::get_base_addr(QString peripheral, QString group, QString field, bool *ok)
{
    for( auto port: m_axi_ports)
    {
        if( (port.peripheral_name != peripheral)
                ||(port.port_name != group))
        {
            continue;
        }

        for(auto fld: port.fields)
        {
            if(fld.field_name == field)
            {
                *ok = true;
                return fld.base_addr;
            }
        }
    }
    *ok = false;
    return 0;
}

// Get a list of the fields (if any) at a given address
QList<QString> Address_map::peripheral_name_from_addr(
                            QVector<Axi_port_t> &axiports, uint32_t addr)
{
    QList<QString> sl;
    for(auto port: axiports)
    {
        if( !((addr >= port.base_addr)
              && (addr < (port.base_addr + port.n_regs))))
            continue;
        for( auto field: port.fields) // fields addresses always ascend
        {
            if(field.base_addr < addr)
            {
                if((field.f_type == Field_t::BitField)
                        || (field.f_type == Field_t::FIFO))
                    continue;
                // Blockram and DistRAM have an extent which may include addr
                if(addr < field.base_addr + field.n_regs)
                {
                    QString txt = QString("%1[0x%2]").arg(field.field_name)
                            .arg(addr-field.base_addr,0,16);
                    sl.push_back(txt);
                    break;
                }
            }
            else if(field.base_addr == addr)
            {
                if((field.f_type == Field_t::BlockRAM)
                        || field.f_type == Field_t::DistRAM)
                {
                    sl.push_back(field.field_name + QString("[0x0]"));
                }
                else
                {
                    sl.push_back(field.field_name);
                }
                continue;
            }
            else if(field.base_addr > addr)
            {
                break;
            }
        }

    }
    return sl;
}

QString  Address_map::reg_descr_from_addr(QVector<Axi_port_t> &axiports, uint32_t addr)
{
    QString str;
    for(auto port: axiports)
    {
        if( !((addr >= port.base_addr)
              && (addr < (port.base_addr + port.n_regs))))
            continue;
        for( auto field: port.fields) // fields addresses always ascend
        {
            if(field.base_addr < addr)
            {
                continue;
            }
            else if (field.base_addr == addr)
            {
                if(field.f_type == Field_t::BitField)
                {
                    if(str.size() != 0)
                        str = QString(" ") + str;
                    str = field.field_name
                            + QString("(%1:%2)").arg(field.bit_base+field.bit_width-1)
                                                .arg(field.bit_base)
                            + str;
                }
            }
            else if(field.base_addr > addr)
            {
                break;
            }
        }

    }
    return str;
}
