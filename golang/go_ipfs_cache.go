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
	core "github.com/ipfs/go-ipfs/core"
	repo "github.com/ipfs/go-ipfs/repo"
	fsrepo "github.com/ipfs/go-ipfs/repo/fsrepo"
	config "github.com/ipfs/go-ipfs/repo/config"
	"github.com/ipfs/go-ipfs/core/coreunix"
)

// #cgo CFLAGS: -DIN_GO=1
//#include <stdlib.h>
//#include <stddef.h>
//#include "../src/data_view_struct.h"
//
//// Don't export these functions into C or we'll get "unused function" warnings.
//// (Or errors saying functions are defined more than once if the're not static)
//
//#if IN_GO
//static void execute_add_callback(void* func, char* data, size_t size, void* arg)
//{
//    ((void(*)(char*, size_t, void*)) func)(data, size, arg);
//}
//static struct data_view* get_nth(struct data_view* dv, size_t n) {
//    return &dv[n];
//}
//#endif // if IN_GO
import "C"

const (
	nBitsForKeypair = 2048
)

func main() {
}

func openOrCreateRepo(ctx context.Context) (repo.Repo, error) {
	repoRoot := "./repo"
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
	pages GenericMap
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

	g.pages = make(GenericMap)

	//s, err := coreunix.Add(g.node, bytes.NewBufferString("halusky"))
	//fmt.Println("Added ", s)
	//<-g.ctx.Done()

	//read, err := coreunix.Cat(ctx, g_node, s)

	//if err != nil {
	//	fmt.Println("Error: failed to cat ", err);
	//}

	//io.Copy(os.Stdout, read)
	//fmt.Println("");

	//<-ctx.Done()
}

func data_to_map(p *C.struct_data_view) interface {} {
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
			m[name] = data_to_map(p.childs);
			return m
		}
	}

	m := make(GenericMap)

	cnt := int(p.child_count)

	for i := 0; i < cnt; i++ {
		ch := C.get_nth(p.childs, C.size_t(i))
		mm := data_to_map(ch).(GenericMap)
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

//export go_ipfs_cache_update_db
func go_ipfs_cache_update_db(dv *C.struct_data_view,
	                         fn unsafe.Pointer, fn_arg unsafe.Pointer) {

	update_db(g.pages, data_to_map(dv).(GenericMap))

	msg, err := json.Marshal(g.pages)
	s, err := coreunix.Add(g.node, bytes.NewReader(msg))

	if err != nil {
		fmt.Println("Error: failed to update db ", err)
	}

	cstr := C.CString(s);
	C.execute_add_callback(fn, cstr, C.size_t(len(s)), fn_arg)
	C.free(unsafe.Pointer(cstr));
}
