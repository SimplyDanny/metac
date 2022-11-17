#include "../repl/completion_trie.h"
#define BASE_IDX_SCALE 1
void CompletionTrie_Init(completion_trie_root_t* self, metac_alloc_t* parentAlloc)
{
    Allocator_Init(&self->TrieAllocator, parentAlloc);

    ARENA_ARRAY_INIT_SZ(completion_trie_node_t, self->Nodes, &self->TrieAllocator, 786)
    ARENA_ARRAY_INIT_SZ(node_range_t, self->NodeRanges, parentAlloc, 512)
    self->NodesCount = 64;

    self->Nodes[0].ChildCount = 0;
    self->Nodes[0].ChildrenBaseIdx = 1;
    self->Nodes[0].Prefix4[0] = 0;
    self->RootCapacity =
        self->NodesCount -
        self->Nodes[0].ChildrenBaseIdx * BASE_IDX_SCALE;

    self->WordCount = 0;
    self->TotalNodes = 1;

    {
        node_range_t range = {0, self->NodesCount, 1};
        ARENA_ARRAY_ADD(self->NodeRanges, range);
    }
}

static int PrefixLen(char prefix4[4])
{
         if (prefix4[0] == '\0')
        return 0;
    else if (prefix4[1] == '\0')
        return 1;
    else if (prefix4[2] == '\0')
        return 2;
    else if (prefix4[3] == '\0')
        return 3;
    else
        return 4;
}


completion_trie_node_t* CompletionTrie_FindLongestMatchingPrefix(completion_trie_root_t* root,
                                                                 const char* word,
                                                                 uint32_t* lengthP)
{
    const completion_trie_node_t const * nodes = root->Nodes;
    const completion_trie_node_t* current = root->Nodes + 0;
    uint32_t length = *lengthP;

    for(;;)
    {
        char c = word[0];
        assert(PrefixLen(nodes[0].Prefix4) == 0);

        const uint32_t childNodeIdx = (current->ChildrenBaseIdx * BASE_IDX_SCALE);
        const uint32_t lastChildNodeIdx =
            (current->ChildrenBaseIdx * BASE_IDX_SCALE) + current->ChildCount;

        int bestPrefixLength = 0;
        int bestChild = 0;

        for(uint32_t i = childNodeIdx; i < lastChildNodeIdx; i++)
        {
            if (c == nodes[i].Prefix4[0])
            {
                int len = PrefixLen(nodes[i].Prefix4);
                if (len > length)
                    continue;

                if (memcmp(nodes[i].Prefix4 + 1, word + 1, len - 1) == 0)
                {
                    if (bestPrefixLength < len)
                    {
                        bestChild = i;
                        bestPrefixLength = len;
                    }
                }
            }
        }

        if (bestChild == 0)
        {
            (*lengthP) = length;
            return current;
        }
        else
        {
            current = nodes + bestChild;
            length -= bestPrefixLength;
            word += bestPrefixLength;
        }
    }

    assert(0);
    return 0;
}

void CompletionTrie_AddChild(completion_trie_root_t* root, completion_trie_node_t* PrefNode,
                             const char* word, uint32_t length)
{
    completion_trie_node_t* child = 0;

    printf("Going to add %.*s\n", (int)length, word);
    INC(root->WordCount);
#if 1
    if (root->Nodes == PrefNode)
    {
        // printf("Starting at root\n");
        if (PrefNode->ChildCount < root->RootCapacity)
        {
            child = root->Nodes +
                (PrefNode->ChildrenBaseIdx
               + INC(PrefNode->ChildCount));
            goto LGotChild;
        }
    }
#endif


    while(length)
    {
        if (PrefNode->ChildCount == 0)
        {
            assert(PrefNode->ChildrenBaseIdx == 0 || PrefNode == root->Nodes + 0);
            if (PrefNode != root->Nodes + 0)
            {
                ARENA_ARRAY_ENSURE_SIZE(root->Nodes, root->NodesCount + BASE_IDX_SCALE);
                PrefNode->ChildrenBaseIdx = (POST_ADD(root->NodesCount, BASE_IDX_SCALE)) / BASE_IDX_SCALE;
                {
                    uint32_t NodeRangestart = PrefNode->ChildrenBaseIdx * BASE_IDX_SCALE;
                    node_range_t range = {NodeRangestart, NodeRangestart + BASE_IDX_SCALE, 1};
                    ARENA_ARRAY_ADD(root->NodeRanges, range);
                    PrefNode->Range = root->NodeRanges + root->NodeRangesCount - 1;
                }
            }
            else
            {
                assert(!"root node grows via standart method .. shouln't happen");
            }
        }

        uint32_t newChildCount = INC(PrefNode->ChildCount) + 1;
        if ((newChildCount % BASE_IDX_SCALE) == 0)
        {
            uint32_t oldChildBaseIdx = PrefNode->ChildrenBaseIdx;
            uint32_t newChildBaseIdx;
            uint32_t newChildCapacity = newChildCount + (BASE_IDX_SCALE - 1);
            const uint32_t endI = newChildCount - 1;

            PrefNode->Range->IsValid = 0;
            ARENA_ARRAY_ENSURE_SIZE(root->Nodes, root->NodesCount + newChildCapacity);
            newChildBaseIdx = POST_ADD(root->NodesCount, newChildCapacity) / BASE_IDX_SCALE;
            {
                uint32_t NodeRangestart = newChildBaseIdx * BASE_IDX_SCALE;
                node_range_t range = {NodeRangestart, NodeRangestart + newChildCapacity, 1};
                ARENA_ARRAY_ADD(root->NodeRanges, range);
                PrefNode->Range = root->NodeRanges + root->NodeRangesCount - 1;
            }
            memcpy(root->Nodes + (newChildBaseIdx * BASE_IDX_SCALE),
                   root->Nodes + (oldChildBaseIdx * BASE_IDX_SCALE),
                   sizeof(*root->Nodes) * (newChildCount - 1));
            PrefNode->ChildrenBaseIdx = newChildBaseIdx;
        }

        child = root->Nodes + ((PrefNode->ChildrenBaseIdx * BASE_IDX_SCALE) + newChildCount - 1);
LGotChild:
        {
            // printf("newChild at node: %u\n", child - root->Nodes);
            INC(root->TotalNodes);

            uint32_t copyLen = 4;
            if (length < 4)
            {
                copyLen = length;
                child->Prefix4[length] = '\0';
            }
            memcpy(child->Prefix4, word, copyLen);
            child->ChildrenBaseIdx = 0;
            child->ChildCount = 0;
            length -= copyLen;
            word += copyLen;
            PrefNode = child;
        }
    }

    if (root->Nodes[0].ChildCount == 0)
    {
        CompletionTrie_PrintRanges(root);
        assert(0);
    }
}

void CompletionTrie_Print(completion_trie_root_t* root, uint32_t n, const char* rootPrefix, FILE* f)
{
    const completion_trie_node_t const * nodes =
        root->Nodes;
    uint32_t i;

    uint32_t childIdxBegin = nodes[n].ChildrenBaseIdx * BASE_IDX_SCALE;
    uint32_t childIdxEnd = childIdxBegin + nodes[n].ChildCount;

    for(i = childIdxBegin; i < childIdxEnd; i++)
    {
        fprintf(f, "  \"%d: %.4s\" -> \"%d: %.4s\"\n",
                n, rootPrefix,
                i, nodes[i].Prefix4);
    }

    fprintf(f, "{ rank = same; ");
    for(i = childIdxBegin; i < childIdxEnd; i++)
    {
        fprintf(f, "\"%d: %.4s\" ", i, nodes[i].Prefix4);
    }
    fprintf(f, "}\n");

    for(i = childIdxBegin; i < childIdxEnd; i++)
    {
        if (nodes[i].ChildCount)
        {
            CompletionTrie_Print(root, i, nodes[i].Prefix4, f);
        }
    }
}

void CompletionTrie_Add(completion_trie_root_t* root, const char* word, uint32_t length)
{
    uint32_t remaning_length = length;
    completion_trie_node_t* PrefNode =
        CompletionTrie_FindLongestMatchingPrefix(root, word, &remaning_length);

    if (remaning_length)
    {
        int posWord = length - remaning_length;
        CompletionTrie_AddChild(root, PrefNode, word + posWord, remaning_length);
    }
    // If the node has children we need to insert a Terminal node
    else if (PrefNode->ChildCount)
    {
        CompletionTrie_AddChild(root, PrefNode, "", 0);
    }

/*
#define MINIMUM(A, B) \
    (((A) < (B)) ? (A) : (B))

    completion_trie_node_t* trie = root->Nodes;

    bool found = false;
    uint32_t pInWord = 0;
    char c = word[0];

    for(;(pInWord < length) && trie->ChildCount != 0;)
    {
        uint32_t i;
        uint32_t endI = trie->ChildrenBaseIdx + trie->ChildCount;
        for(i = trie->ChildrenBaseIdx; i < endI; i++)
        {
            uint32_t bestMatch = 0;
            if (root->Nodes[i].Prefix4[0] == c)
            {
                uint32_t j;
                uint32_t jEnd = MINIMUM(3, length - pInWord - 1);
                for(j = 1; i < jEnd; j++)
                {
                    if (word[pInWord + j] != root->Nodes[i].Prefix4[j])
                    {
                        uint32_t lastMatchJ = ((bestMatch & 0xFFFF) >> 1);
                        if (lastMatchJ < j)
                        {
                            bestMatch = (i << 16 | j << 1 | 0);
                        }
                        else if (lastMatchJ == j)
                        {
                            bestMatch |= 1;
                        }
                        goto LconinueChildren;
                    }
                }
                // if we get here we have found a child with matching prefix
                {
                    pInWord +=
                        PrefixLen(root->Nodes[i].Prefix4);
                    trie = root->Nodes + i;
                    i = 0;
                }
            }
        LconinueChildren: continue;
        }
        //
    }
    // when we end up here trie should be the place where we insert ourselfs
    // in the case where tire doesn't have payload we can write ourselfs into it
    if (trie->Prefix4[0] == '\0')
    {
        int32_t lenToWrite = (int32_t)(length - pInWord);

        for(;lenToWrite >= 4; lenToWrite -= 4)
        {
            trie->Prefix4[0] = word[pInWord + 0];
            trie->Prefix4[1] = word[pInWord + 1];
            trie->Prefix4[2] = word[pInWord + 2];
            trie->Prefix4[3] = word[pInWord + 3];
            // if ()
        }

        if(lenToWrite > 4)
        {

        }
    }
    else
    {

    }
#undef MINIMUM
*/
}

void AddIdentifierToCompletionSet(const char* idStr, uint32_t idKey, void* ctx)
{
    completion_trie_node_t* Trie = (completion_trie_node_t*) ctx;

    uint32_t length = LENGTH_FROM_IDENTIFIER_KEY(idKey);
}

void testCompletionTrie(void)
{
    metac_alloc_t rootAlloc;
    Allocator_Init(&rootAlloc, 0);

    completion_trie_root_t root;

    CompletionTrie_Init(&root, &rootAlloc);
}

void CompletionTrie_PrintRanges(completion_trie_root_t* self)
{
    FILE* f = fopen("ranges.txt", "w");
    const uint32_t nodeRangesCount =  self->NodeRangesCount;
    for(uint32_t i = 0; i < nodeRangesCount; i++)
    {
        node_range_t range = self->NodeRanges[i];
        fprintf(f, "%sRange:{%d, %d}\n",
                   (range.IsValid ? "" : "- "), range.Begin, range.End);
    }
    fclose(f);
}

void CompletionTrie_PrintStats(completion_trie_root_t* self)
{
    {
        FILE* f = fopen("g.dot", "w");
        fprintf(f, "digraph G {\n");
        fprintf(f, "  node [shape=record headport=n]\n");

        CompletionTrie_Print(self, 0, "", f);

        fprintf(f, "}\n");
        fclose(f);
    }

    printf("UsedNodes: %u\n", self->TotalNodes);
    printf("AllocatedNodes: %u\n", self->NodesCount);
    printf("Words: %u\n", self->WordCount);

    int AllocatedNodesPerWord = ((float)self->NodesCount / (float)self->WordCount) * 100.0f;
    int UsedNodesPerWord = ((float)self->TotalNodes / (float)self->WordCount) * 100.0f;

    printf("UsedNodesPerWord: %d.%d\n", UsedNodesPerWord / 100, UsedNodesPerWord % 100);
    printf("AllocatedNodesPerWord: %d.%d\n", AllocatedNodesPerWord / 100, AllocatedNodesPerWord % 100);

    CompletionTrie_PrintRanges(self);
}
