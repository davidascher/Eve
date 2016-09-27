#include <unix_internal.h>
    
typedef struct udp_bag {
    struct bag b;
    heap h;
    table channels;
    evaluation ev;
    udp sender;
} *udp_bag;


static CONTINUATION_1_5(udp_scan, udp_bag, int, listener, value, value, value);
static void udp_scan(udp_bag ub, int sig, listener out, value e, value a, value v)
{
    prf("udp scan: %v %v %v\n", e, a, v);
    if ((sig == s_eAV) && (a == sym(tag)) && (v == sym(udp))){
        table_foreach(ub->channels, e, u)
            apply(out, e, a, v, 1, 0);
    }
    
    // ech
    if ((sig == s_eAv) && (a == sym(address)))  
        table_foreach(ub->channels, e, u)
            apply(out, e, a, udp_station(u), 1, 0);

    
    if ((sig == s_EAv) && (a == sym(address))) {
        udp u = table_find(ub->channels, e);
        apply(out, e, a, udp_station(u), 1, 0);
    }
}

static CONTINUATION_1_2(udp_input, udp_bag, station, buffer);
static void udp_input(udp_bag ub, station s, buffer b)
{
    prf ("packet input\n");
}

static CONTINUATION_1_1(udp_commit, udp_bag, edb);
// oh the shame
static void udp_commit(udp_bag ub, edb s)
{
    station d;
    prf("udp commit: %b\n", edb_dump(init, s));

    edb_foreach_e(s, e, sym(tag), sym(udp), c) {
        unsigned int host = 0;
        int port = 0;
        edb_foreach_v(s, e, sym(port), pt, c) {
            port = (int)*(double *)pt;
            // fill in port if defined
        }
        // as ascii i think i guess
        edb_foreach_v(s, e, sym(host), h, c) {
            // fill in host if defined
        }
        prf("udp commit %d\n", table_elements(ub->b.listeners));
        udp u = create_udp(ub->h, create_station(host, port), cont(ub->h, udp_input, ub));
        table_set(ub->channels, e, u);
    }

    edb_foreach_e(s, e, sym(tag), sym(packet), _) {
        edb_foreach_v(s, e, sym(destination), destination, _) {
            edb_foreach_v(s, e, sym(body), b, _) {
                edb packet = table_find(ub->ev->t_input, b);
                if (packet) {
                    buffer b = allocate_buffer(ub->h, 10);
                    serialize_edb(b, packet);
                    // send to a particular endpoint
                    prf("upd send %v %d\n", destination, buffer_length(b));
                    udp_write(ub->sender, destination, b);
                }
            }
        }
    }
}

static CONTINUATION_1_2(udp_reception, udp_bag, station, buffer);
static void udp_reception(udp_bag u, station s, buffer b)
{
    prf("input - maka bag\n");
    uuid p = generate_uuid();
    bag in = (bag)create_edb(u->h, 0);
    apply(deserialize_into_bag(u->h, in), b, ignore);
    table_foreach(((bag)u)->listeners, t, _) 
        apply((bag_handler)t, in);
}

// we shouldn't need this evaluation, but first class bags and
// hackerism
bag udp_bag_init(evaluation ev)
{
    // this should be some kind of parameterized listener.
    // we can do the same trick that we tried to do
    // with time, by creating an open read, but it
    // has strange consequences. sidestep by just
    // having an 'eve port'
    heap h = allocate_rolling(pages, sstring("udp bag"));
    udp_bag ub = allocate(h, sizeof(struct udp_bag));
    ub->h = h;
    ub->b.commit = cont(h, udp_commit, ub);
    ub->b.scan = cont(h, udp_scan, ub);
    ub->b.listeners = allocate_table(h, key_from_pointer, compare_pointer);
    ub->b.blocks = allocate_vector(h, 0);
    ub->b.block_listeners = allocate_table(h, key_from_pointer, compare_pointer);
    ub->channels = create_value_table(h);
    ub->ev = ev;
    ub->sender = create_udp(h, ip_wildcard, cont(h, udp_input, ub)); 
    return (bag)ub;
}
