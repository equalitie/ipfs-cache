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
	"time"
	"io"
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
//
//// Don't export these functions into C or we'll get "unused function" warnings.
//// (Or errors saying functions are defined more than once if the're not static)
//
//#if IN_GO
//static void execute_void_cb(void* func, void* arg)
//{
//    ((void(*)(void*)) func)(arg);
//}
//static void execute_data_cb(void* func, void* data, size_t size, void* arg)
//{
//    ((void(*)(char*, size_t, void*)) func)(data, size, arg);
//}
//#endif // if IN_GO
import "C"

const (
	nBitsForKeypair = 2048
	repoRoot = "./repo"
)

func main() {
}

func doesnt_exist_or_is_empty(path string) bool {
	f, err := os.Open(path)
	if err != nil {
		if os.IsNotExist(err) {
			return true
		}
		return false
	}
	defer f.Close()

	_, err = f.Readdirnames(1)
	if err == io.EOF {
		return true
	}
	return false
}

func openOrCreateRepo(ctx context.Context) (repo.Repo, error) {
	if doesnt_exist_or_is_empty(repoRoot) {
		conf, err := config.Init(os.Stdout, nBitsForKeypair)

		if err != nil {
			return nil, err
		}

		if err := fsrepo.Init(repoRoot, conf); err != nil {
			return nil, err
		}
	}

	return fsrepo.Open(repoRoot)
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

type Cache struct {
	node *core.IpfsNode
	ctx context.Context
	cancel context.CancelFunc
}

var g Cache

//export go_ipfs_cache_start
func go_ipfs_cache_start() bool {
	g.ctx, g.cancel = context.WithCancel(context.Background())

	r, err := openOrCreateRepo(g.ctx);

	if err != nil {
		fmt.Println("err", err);
		return false
	}

	g.node, err = core.NewNode(g.ctx, &core.BuildCfg{
		Online: true,
		Permament: true,
		Repo:   r,
	})

	g.node.SetLocal(false)

	printSwarmAddrs(g.node)

	return true
}

//export go_ipfs_cache_stop
func go_ipfs_cache_stop() {
	g.cancel()
}

func resolve(ctx context.Context, n *core.IpfsNode, ipns_id string) (string, error) {
	p := path.Path("/ipns/" + ipns_id)
	node, err := core.Resolve(ctx, n.Namesys, n.Resolver, p)
	if err != nil { return "", err }

	return node.Cid().String(), nil
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

//export go_ipfs_cache_publish
func go_ipfs_cache_publish(cid *C.char, fn unsafe.Pointer, fn_arg unsafe.Pointer) {
	go func() {
		id := C.GoString(cid)
		publish(g.ctx, g.node, id);
		C.execute_void_cb(fn, fn_arg)
	}()
}

//export go_ipfs_cache_insert_content
func go_ipfs_cache_insert_content(
		data unsafe.Pointer, size C.size_t,
		fn unsafe.Pointer, fn_arg unsafe.Pointer) {
	go func() {
		fmt.Println("go_opfs_cache_insert_content");

		msg := C.GoBytes(data, C.int(size))
		cid, err := coreunix.Add(g.node, bytes.NewReader(msg))

		if err != nil {
			fmt.Println("Error: failed to insert content ", err)
			C.execute_data_cb(fn, nil, C.size_t(0), fn_arg)
			return;
		}

		cdata := C.CBytes([]byte(cid))
		defer C.free(cdata)

		C.execute_data_cb(fn, cdata, C.size_t(len(cid)), fn_arg)
	}()
}

//export go_ipfs_get_content
func go_ipfs_get_content(c_cid *C.char, fn unsafe.Pointer, fn_arg unsafe.Pointer) { //([]byte, error) {
	go func() {
		cid := C.GoString(c_cid)

		reader, err := coreunix.Cat(g.ctx, g.node, cid)

		if err != nil {
			C.execute_data_cb(fn, nil, C.size_t(0), fn_arg)
			return
		}

		bytes, err := ioutil.ReadAll(reader)
		if err != nil {
			C.execute_data_cb(fn, nil, C.size_t(0), fn_arg)
			return
		}

		cdata := C.CBytes(bytes)
		defer C.free(cdata)

		C.execute_data_cb(fn, cdata, C.size_t(len(bytes)), fn_arg)
	}()
}

