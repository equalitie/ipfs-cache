// Useful links:
// https://github.com/ipfs/go-ipfs/issues/3060
// https://github.com/ipfs/examples/tree/master/examples

package main

import (
	"fmt"
	"context"
	"os"
	"bytes"
	"sort"
	"unsafe"
	"reflect"
	"encoding/json"
	"time"
	"io/ioutil"
	core "github.com/ipfs/go-ipfs/core"
	repo "github.com/ipfs/go-ipfs/repo"
	fsrepo "github.com/ipfs/go-ipfs/repo/fsrepo"
	config "github.com/ipfs/go-ipfs/repo/config"
	path "github.com/ipfs/go-ipfs/path"
	"github.com/ipfs/go-ipfs/core/coreunix"

	peer "gx/ipfs/QmXYjuNuxVzXKJCfWasQk1RqkhVLDM9jtUKhqc2WPQmFSB/go-libp2p-peer"
)

// #cgo CFLAGS: -DIN_GO=1
//#include <stdlib.h>
//#include <stddef.h>
//#include "../src/query_view_struct.h"
//
//// Don't export these functions into C or we'll get "unused function" warnings.
//// (Or errors saying functions are defined more than once if the're not static)
//
//#if IN_GO
//static void execute_add_callback(void* func, char* data, size_t size, void* arg)
//{
//    ((void(*)(char*, size_t, void*)) func)(data, size, arg);
//}
//static struct query_view* get_nth(struct query_view* dv, size_t n) {
//    return &dv[n];
//}
//#endif // if IN_GO
import "C"

const (
	nBitsForKeypair = 2048
	repoRoot = "./repo"
	dbFile = repoRoot + "/db.cid"
)

func main() {
}

func openOrCreateRepo(ctx context.Context) (repo.Repo, error) {
	r, err := fsrepo.Open(repoRoot)

	if err != nil {
		conf, err := config.Init(os.Stdout, nBitsForKeypair)

		if err != nil {
			return nil, err
		}

		if err := fsrepo.Init(repoRoot, conf); err != nil {
			return nil, err
		}
	}

	return r, nil
}

func printSwarmAddrs(node *core.IpfsNode) {
	if !node.OnlineMode() {
		fmt.Println("Swarm not listening, running in offline mode.")
		return
	}
	var addrs []string
	for _, addr := range node.PeerHost.Addrs() {
		addrs = append(addrs, addr.String())
	}
	sort.Sort(sort.StringSlice(addrs))

	for _, addr := range addrs {
		fmt.Printf("Swarm listening on %s\n", addr)
	}
}

type GenericMap map[string]interface{}

type Cache struct {
	node *core.IpfsNode
	ctx context.Context
	db GenericMap
}

var g Cache

//export go_ipfs_cache_start
func go_ipfs_cache_start() {
	g.ctx = context.Background()

	r, err := openOrCreateRepo(g.ctx);

	if err != nil {
		fmt.Println("err", err);
	}

	g.node, err = core.NewNode(g.ctx, &core.BuildCfg{
		Online: true,
		Permament: true,
		Repo:   r,
	})

	g.node.SetLocal(false)

	printSwarmAddrs(g.node)

	g.db, err = load_db(g.ctx)

	if err != nil { g.db = make(GenericMap) }
}

func get_content(ctx context.Context, cid string) ([]byte, error) {
	reader, err := coreunix.Cat(ctx, g.node, cid)
	if err != nil { return nil, err }

	bytes, err := ioutil.ReadAll(reader)
	if err != nil { return nil, err }

	return bytes, nil
}

func load_db(ctx context.Context) (GenericMap, error) {
	cid, err := ioutil.ReadFile(dbFile);
	if err != nil { return nil, err }

	json_data, err := get_content(ctx, string(cid[:]))
	if err != nil { return nil, err }

	var m GenericMap
	err = json.Unmarshal(json_data, &m)
	if err != nil { return nil, err }

	return m, nil
}

func resolve(ctx context.Context, n *core.IpfsNode, ipns_id string) (string, error) {
	p := path.Path("/ipns/" + ipns_id)
	node, err := core.Resolve(ctx, n.Namesys, n.Resolver, p)
	if err != nil { return "", err }

	return node.Cid().String(), nil
}

func query_to_map(p *C.struct_query_view) interface {} {
	if p.str_size != 0 {
		// Either string or entry.
		if p.child_count == 0 {
			// String
			s := C.GoStringN(p.str, C.int(p.str_size))
			if p.child_count != 0 {
				panic(fmt.Sprintf("String must have zero children"));
			}
			return s
		} else {
			// Entry
			name := C.GoStringN(p.str, C.int(p.str_size))
			if p.child_count != 1 {
				panic(fmt.Sprintf("Entry must have exactly one child"));
			}
			m := make(GenericMap);
			m[name] = query_to_map(p.childs);
			return m
		}
	}

	m := make(GenericMap)

	cnt := int(p.child_count)

	for i := 0; i < cnt; i++ {
		ch := C.get_nth(p.childs, C.size_t(i))
		mm := query_to_map(ch).(GenericMap)
		for key, value := range mm {
			m[key] = value
		}
	}

	return m;
}

func is_same_type(a interface{}, b interface{}) bool {
	return reflect.TypeOf(a) == reflect.TypeOf(b)
}

func update_db(db GenericMap, query GenericMap) {
	for key, q_v := range query {
		db_v, ok := db[key]

		if ok && is_same_type(db_v, q_v) {
			if m, ok := db_v.(GenericMap); ok {
				update_db(m, q_v.(GenericMap));
			}
		} else {
			db[key] = q_v
		}
	}
}

// IMPORTANT: The returned value needs to be `free`d.
//export go_ipfs_cache_ipns_id
func go_ipfs_cache_ipns_id() *C.char {
	pid, err := peer.IDFromPrivateKey(g.node.PrivateKey)

	if err != nil {
		return nil
	}

	cstr := C.CString(pid.Pretty())
	return cstr
}

func publish(ctx context.Context, n *core.IpfsNode, cid string) error {
	path, err := path.ParseCidToPath(cid)

	if err != nil {
		return err
	}

	k := n.PrivateKey

	// TODO: What should be the default timeout?
	eol := time.Now().Add(3 * time.Minute)
	err  = n.Namesys.PublishWithEOL(ctx, k, path, eol)

	if err != nil {
		return err
	}

	return nil
}

//export go_ipfs_cache_update_db
func go_ipfs_cache_update_db(dv *C.struct_query_view,
	                         fn unsafe.Pointer, fn_arg unsafe.Pointer) {

	update_db(g.db, query_to_map(dv).(GenericMap))

	msg, err := json.Marshal(g.db)
	cid, err := coreunix.Add(g.node, bytes.NewReader(msg))

	if err != nil {
		fmt.Println("Error: failed to update db ", err)
		C.execute_add_callback(fn, nil, C.size_t(0), fn_arg)
		return
	}

	err = ioutil.WriteFile(dbFile, []byte(cid), 0644)

	if err != nil {
		fmt.Println("Error: writing db into a local file ", err)
	}

	publish(g.ctx, g.node, cid)

	cstr := C.CString(cid)
	C.execute_add_callback(fn, cstr, C.size_t(len(cid)), fn_arg)
	C.free(unsafe.Pointer(cstr))
}

//export go_ipfs_cache_insert_content
func go_ipfs_cache_insert_content(
		data unsafe.Pointer, size C.size_t,
		fn unsafe.Pointer, fn_arg unsafe.Pointer) {

	msg := C.GoBytes(data, C.int(size))
	cid, err := coreunix.Add(g.node, bytes.NewReader(msg))

	if err != nil {
		fmt.Println("Error: failed to insert content ", err)
		C.execute_add_callback(fn, nil, C.size_t(0), fn_arg)
		return;
	}

	cstr := C.CString(cid)
	C.execute_add_callback(fn, cstr, C.size_t(len(cid)), fn_arg)
	C.free(unsafe.Pointer(cstr))
}
