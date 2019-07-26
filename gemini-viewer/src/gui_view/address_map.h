#ifndef ADDRESS_MAP_H
#define ADDRESS_MAP_H

#include <QString>
#include <QList>
#include <QVector>

enum class Access_t  {RW = 0, RO, WO};
enum class Field_t {BitField=0, DistRAM, BlockRAM, FIFO};

struct Args_field_t
{
    uint32_t base_addr;
    Field_t  f_type;
    Access_t rw;
    uint32_t n_regs;
    uint32_t bit_base;
    uint32_t bit_width;
    QString field_name;
    // To allow fields to be sorted into address order
    bool operator< (const Args_field_t rhs) const;
};

struct Axi_port_t
{
    uint32_t base_addr;
    uint32_t n_regs;
    QString peripheral_name;
    QString port_name;
    QVector<Args_field_t> fields;
    // to allow ports to be sorted into address order
    bool operator< (const Axi_port_t rhs) const;
};

class Address_map
{
private:
    QString m_filename;
    bool is_loaded;
    QVector<Axi_port_t> m_axi_ports;

    bool process_line(QString line);
    QList<QString> tokenise(QString line);
    bool make_bitfield(QList<QString> &tokens);
    bool make_distram(QList<QString> &tokens);
    bool make_blockram(QList<QString> &tokens);
    bool make_fifo(QList<QString> &tokens);
    void save_field( Args_field_t f, QString &peripheral_name, QString &port_name);
    void dump();
    void check_holes();

public:
    Address_map(QString filename);
    ~Address_map();
    bool load();
    bool isLoadedOk();
    int get_num_ports();
    Axi_port_t * get_port(int i);
    QVector<Axi_port_t> & all_ports();
    uint32_t get_base_addr(QString peripheral, QString group, QString field
                           , bool *ok);
    QList<QString> peripheral_name_from_addr(QVector<Axi_port_t> &axiports, uint32_t addr);
    QString reg_descr_from_addr(QVector<Axi_port_t> &axiports, uint32_t addr);
};

#endif // ADDRESS_MAP_H
