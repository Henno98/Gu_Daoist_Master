Gu Procedural Species Compiler v2

Replace these files in Source/Gu_Daoist_Master/:
- GuProceduralGeneratorSubsystem.h
- GuProceduralGeneratorSubsystem.cpp
- RefinementSubsystem.cpp

Core behavior:
- Existing generator:v1 runtime species remain on the legacy compiler path.
- New procedural species use generator:v2.
- New divergent/experimental refinement species use compiler:semantic-v2.
- V2 adds deterministic structural diversity modules on top of path/role composition.
- Refinement surviving attributes bias role affinity and structural modifier selection.
- Generated names and appearance descriptors have broader deterministic grammar.
- GenerateSpeciesBatch can compile 1..5000 species definitions without creating physical Gu instances.
- FProceduralGuGenerationResult now reports StructureSignature and MechanicTypes for diversity inspection.

Recommended first test:
1. GenerateSpeciesBatch with one path, Rank 1-3, Count 100, fixed BatchSeed.
2. Inspect DefinitionId, Name, StructureSignature, MechanicTypes.
3. Run the same batch again with the same BatchSeed and confirm the same DefinitionIds are returned/reused.
4. Run an experimental refinement and confirm the resulting runtime Gu still enters the aperture and binds through the existing runtime Gu bridge.

This source was patched against the exact Gu_Daoist_Master.zip uploaded in the conversation. No installer is included.
