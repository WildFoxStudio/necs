

#include <array>
#include "limits"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "necs/CPagedAllocator.h"
#include "necs/CEntityFactory.h"
#include "necs/CWorldObject.h"
#include "necs/CMatrixAllocator.h"

#pragma region CPagedAllocator

class CAlignedAllocatorMock : public IAlignedAllocator
{
public:
	MOCK_METHOD(void*, Allocate, (const uint64_t, const uint64_t), (override));
	MOCK_METHOD(void, Free, (void*), (override));
};

// Test fixture for reusing common setup and teardown logic
class CPagedAllocatorFixture : public ::testing::Test {
protected:
	inline static constexpr uint64_t NUM_OF_OBJECTS{ 10 };
	inline static constexpr uint64_t SIZE_OF_OBJECTS{ 32 };

	CPagedAllocator<CAlignedAllocatorMock>* AllocatorUnderTest{};
	alignas(max_align_t) std::array<uint8_t, NUM_OF_OBJECTS* SIZE_OF_OBJECTS * sizeof(freelist_block)> Buffer{};

	void SetUp() override {
		AllocatorUnderTest = new CPagedAllocator<CAlignedAllocatorMock>(NUM_OF_OBJECTS, SIZE_OF_OBJECTS);
	}

	void TearDown() override {
		delete AllocatorUnderTest;
	}

	void SetAllocatorNoCalls()
	{
		EXPECT_CALL(AllocatorUnderTest->_alignedAllocator, Allocate).Times(0);
		EXPECT_CALL(AllocatorUnderTest->_alignedAllocator, Free).Times(0);
	}

	void SetAllocatorReturnsNullptr()
	{
		EXPECT_CALL(AllocatorUnderTest->_alignedAllocator, Allocate).Times(1).WillOnce(::testing::Return(nullptr));
		EXPECT_CALL(AllocatorUnderTest->_alignedAllocator, Free).Times(0);
	}

	void SetAllocatorReturnsBuffer(uint64_t times)
	{
		EXPECT_CALL(AllocatorUnderTest->_alignedAllocator, Allocate).Times(times).WillRepeatedly(::testing::Return(Buffer.data()));
		EXPECT_CALL(AllocatorUnderTest->_alignedAllocator, Free).Times(times);
	}
};

TEST_F(CPagedAllocatorFixture, DoesntAllocateOnConstruction) {
	SetAllocatorNoCalls();
}

TEST_F(CPagedAllocatorFixture, WhenAllocatorReturnsNullptrBadAllocMustBeThrown) {
	SetAllocatorReturnsNullptr();

	EXPECT_ANY_THROW(AllocatorUnderTest->Allocate());
}

// Using the test fixture
TEST_F(CPagedAllocatorFixture, AllocatesAlignedBlocks) {
	std::array<uintptr_t, NUM_OF_OBJECTS> allocations{};

	/*Only 1 page/arena is allocated*/
	SetAllocatorReturnsBuffer(1);

	for (uint64_t i{}; i < NUM_OF_OBJECTS; i++)
	{
		const uintptr_t buffer_ptr{ reinterpret_cast<uintptr_t>(Buffer.data()) };
		const uintptr_t expected_ptr{ buffer_ptr + i * SIZE_OF_OBJECTS * sizeof(max_align_t) };
		allocations[i] = reinterpret_cast<uintptr_t>(AllocatorUnderTest->Allocate());
		EXPECT_NE(allocations[i], 0ull);
		/*Each allocation must be aligned to max_align_t*/
		EXPECT_EQ(allocations[i] % sizeof(max_align_t), 0);
	}
}

// Using the test fixture
TEST_F(CPagedAllocatorFixture, AllocateThenFreeMultiplePages) {
	constexpr uint64_t EXPECTED_NUM_OF_PAGES{ 100 };
	/*Only 1 page/arena is allocated*/
	SetAllocatorReturnsBuffer(EXPECTED_NUM_OF_PAGES);

	for (uint64_t i{}; i < NUM_OF_OBJECTS * EXPECTED_NUM_OF_PAGES; i++)
	{
		AllocatorUnderTest->Allocate();
	}

	delete AllocatorUnderTest;
	AllocatorUnderTest = nullptr;
}


#pragma region CMatrixAllocator

class CPagedAllocatorStub final : public IPagedAllocator
{
public:
	CPagedAllocatorStub(const uint64_t maxNumOfElementsPerSlab, const uint64_t elementSize) :IPagedAllocator(maxNumOfElementsPerSlab, elementSize), BlockSize(elementSize) {
	};

	void* Allocate() override { return (void*)&BlockSize; };
	void Free(void* ptr) override {};
	uint64_t GetFixedBlockSize()const override { return BlockSize; };

	volatile const uint64_t BlockSize;
};

TEST(CMatrixAllocatorTest, MustDieIfZeroIsPassed)
{
	EXPECT_DEATH(CMatrixAllocator<CPagedAllocatorStub> allocator(0), ".*");
}

TEST(CMatrixAllocatorTest, MustDieWhenAllocatingZeroBytes)
{
	CMatrixAllocator<CPagedAllocatorStub> allocator(1);
	EXPECT_DEATH(allocator.Allocate(0), ".*");
}

TEST(CMatrixAllocatorTest, ShouldAllocateCorrectPageForDifferentSizeTypes)
{
	CMatrixAllocator<CPagedAllocatorStub> allocator(1);
	uint64_t expectedNumOfAllocators{};
	for (uint64_t i{ 1 }; i < UINT32_MAX; i += UINT32_MAX / 10)
	{
		const uint64_t* const blockSize{ reinterpret_cast<const uint64_t* const>(allocator.Allocate(i)) };
		EXPECT_EQ(*blockSize, i);
		EXPECT_EQ(allocator._perSizeAllocator.size(), ++expectedNumOfAllocators);
	}
}

#pragma endregion

#pragma region CEntityFactory
// Test fixture for reusing common setup and teardown logic
class CEntityFactoryFixture : public ::testing::Test {
protected:
	struct CTestWorldObject : public CWorldObject
	{
		CTestWorldObject(const DWorldObjectInitializer& init) : CWorldObject(init, false) {}
		double A{};
	};

	CEntityFactory* FactoryUnderTest{};

	void SetUp() override {
		FactoryUnderTest = new CEntityFactory();
	}

	void TearDown() override {
		delete FactoryUnderTest;
	}

	const std::unordered_map<std::string, std::pair<CEntityFactory::CreateFunc, std::unique_ptr<CWorldObject>>>& GetClassNamesMap()const { return FactoryUnderTest->_classNameToCreateFnAndCdo; }
};

TEST_F(CEntityFactoryFixture, MustRegisterClassName) {
	FactoryUnderTest->RegisterEntityClass<CTestWorldObject>("CTestWorldObject");

	EXPECT_NE(GetClassNamesMap().find("CTestWorldObject"), GetClassNamesMap().end());
};

TEST_F(CEntityFactoryFixture, MustThrowIfRegisterClassNameTwice) {
	FactoryUnderTest->RegisterEntityClass<CTestWorldObject>("CTestWorldObject");
	EXPECT_DEATH(FactoryUnderTest->RegisterEntityClass<CTestWorldObject>("CTestWorldObject"), ".*");
};

TEST_F(CEntityFactoryFixture, MustAssertIfMemoryIsNullptr) {
	FactoryUnderTest->RegisterEntityClass<CTestWorldObject>("CTestWorldObject");
	EXPECT_DEATH(FactoryUnderTest->PlacementNewFromTypename(nullptr, nullptr, "CTestWorldObject"), ".*");
};

TEST_F(CEntityFactoryFixture, MustAssertIfPendingDestroyedIsNullptr) {
	FactoryUnderTest->RegisterEntityClass<CTestWorldObject>("CTestWorldObject");
	EXPECT_DEATH(FactoryUnderTest->PlacementNewFromTypename(nullptr, nullptr, "CTestWorldObject"), ".*");
};

TEST_F(CEntityFactoryFixture, MustNotAssertWhenQueryingClassCDO) {
	FactoryUnderTest->RegisterEntityClass<CTestWorldObject>("CTestWorldObject");

	EXPECT_NO_FATAL_FAILURE(FactoryUnderTest->GetCDOFromTypename("CTestWorldObject"));
};

TEST_F(CEntityFactoryFixture, MustAssertWhenQueryingNonRegisteredClassCDO) {
	EXPECT_DEATH(FactoryUnderTest->GetCDOFromTypename("CTestWorldObject"), ".*");
};

TEST_F(CEntityFactoryFixture, MustReturnCorrectCDOData) {
	FactoryUnderTest->RegisterEntityClass<CTestWorldObject>("CTestWorldObject");

	EXPECT_EQ(GetClassNamesMap().find("CTestWorldObject")->second.second->GetClassSize(), sizeof(CTestWorldObject));
	EXPECT_EQ(GetClassNamesMap().find("CTestWorldObject")->second.second->GetClassAlignment(), alignof(CTestWorldObject));
	EXPECT_EQ(GetClassNamesMap().find("CTestWorldObject")->second.second->ComputeComponentsMaxSizeForAllocation(), 0u);
	EXPECT_EQ(GetClassNamesMap().find("CTestWorldObject")->second.second->GetCDOComponentsInfo().size(), 0u);
};

#pragma endregion

#pragma region CWorldObject

TEST(CWorldObjectCDOZeroSizeTest, MustAssertOnZeroClassSizeWhenCDO) {
	EXPECT_DEATH(CWorldObjectCDO(true, 0, 0), ".*");
};

TEST(CWorldObjectCDOZeroSizeTest, MustAssertOnZeroClassSizeWhenNotCDO) {
	EXPECT_DEATH(CWorldObjectCDO(false, 0, 0), ".*");
};

class CWorldObjectCDOFixture : public ::testing::Test {
protected:
	inline static constexpr uint64_t CLASS_SIZE{ 32 };
	inline static constexpr uint64_t CLASS_ALIGNMENT{ 8 };
	CWorldObjectCDO* cdo{};

	void SetUp() override {
		cdo = new CWorldObjectCDO(true, CLASS_SIZE, CLASS_ALIGNMENT);
	}

	void TearDown() override {
		delete cdo;
	}

};

TEST_F(CWorldObjectCDOFixture, MustReturnIsCdoTrue) {
	EXPECT_EQ(cdo->IsCDO(), true);
};

TEST_F(CWorldObjectCDOFixture, MustHaveExpectedClassSize) {
	EXPECT_EQ(cdo->GetClassSize(), CLASS_SIZE);
};

TEST_F(CWorldObjectCDOFixture, MustHaveExpectedClassAlignment) {
	EXPECT_EQ(cdo->GetClassAlignment(), CLASS_ALIGNMENT);
};

TEST_F(CWorldObjectCDOFixture, MustReturnZeroBytesForZeroComponents) {
	EXPECT_EQ(cdo->ComputeComponentsMaxSizeForAllocation(), 0);
};

TEST_F(CWorldObjectCDOFixture, MustReturnEmptyComponentsInfo) {
	EXPECT_EQ(cdo->GetCDOComponentsInfo().size(), 0);
};

TEST_F(CWorldObjectCDOFixture, MustReturnOneComponentsInfoForTemplateComponent) {
	struct Component
	{
		uint32_t a, b, c, d;
	};
	constexpr uint64_t COMPONENT_SIZE{ sizeof(Component) };
	constexpr uint64_t COMPONENT_ALIGNMENT{ alignof(Component) };
	cdo->StaticRegisterNewComponent<Component>();

	EXPECT_EQ(cdo->GetCDOComponentsInfo().size(), 1u);
	EXPECT_EQ(cdo->GetCDOComponentsInfo().at(0).Size, COMPONENT_SIZE);
	EXPECT_EQ(cdo->GetCDOComponentsInfo().at(0).Alignment, COMPONENT_ALIGNMENT);
};

TEST_F(CWorldObjectCDOFixture, MustReturnOneComponentsInfoForUnkownComponent) {
	constexpr uint64_t COMPONENT_SIZE{ 16 };
	constexpr uint64_t COMPONENT_ALIGNMENT{ 4 };
	cdo->StaticRegisterNewComponentUnknown(COMPONENT_SIZE, COMPONENT_ALIGNMENT);

	EXPECT_EQ(cdo->GetCDOComponentsInfo().size(), 1u);
	EXPECT_EQ(cdo->GetCDOComponentsInfo().at(0).Size, COMPONENT_SIZE);
	EXPECT_EQ(cdo->GetCDOComponentsInfo().at(0).Alignment, COMPONENT_ALIGNMENT);
};

TEST_F(CWorldObjectCDOFixture, MustAssertIfComponentSizeIsZero) {

	EXPECT_DEATH(cdo->StaticRegisterNewComponentUnknown(0, 2), ".*");
};

TEST_F(CWorldObjectCDOFixture, MustAssertIfComponentAlignmentIsZero) {

	EXPECT_DEATH(cdo->StaticRegisterNewComponentUnknown(2, 0), ".*");
};

TEST_F(CWorldObjectCDOFixture, MustNotAssertIfComponentAlignmentIsOne) {

	EXPECT_NO_FATAL_FAILURE(cdo->StaticRegisterNewComponentUnknown(2, 1));
};

TEST_F(CWorldObjectCDOFixture, MustAssertIfComponentAlignmentIsNonPowerOfTwo) {

	EXPECT_DEATH(cdo->StaticRegisterNewComponentUnknown(4, 3), ".*");
};

TEST_F(CWorldObjectCDOFixture, MustNotAssertIfComponentAlignmentIsPowerOfTwo) {

	EXPECT_NO_FATAL_FAILURE(cdo->StaticRegisterNewComponentUnknown(16, 16));
};

TEST_F(CWorldObjectCDOFixture, MustAssertIfComponentAlignmentIsLargerThanClassSize) {

	EXPECT_DEATH(cdo->StaticRegisterNewComponentUnknown(4, 8), ".*");
};

TEST_F(CWorldObjectCDOFixture, MustReturnMultipleComponentsInfo) {
	constexpr uint64_t COMPONENT_SIZE{ 16 };
	constexpr uint64_t COMPONENT_ALIGNMENT{ 4 };
	constexpr uint64_t NUM_COMPONENTS{ 100 };

	for (uint64_t i{}; i < NUM_COMPONENTS; i++)
	{
		EXPECT_EQ(cdo->GetCDOComponentsInfo().size(), i);
		cdo->StaticRegisterNewComponentUnknown(COMPONENT_SIZE * (i + 1), COMPONENT_ALIGNMENT);
		EXPECT_EQ(cdo->GetCDOComponentsInfo().size(), (i + 1));
	}

	for (uint64_t i{}; i < NUM_COMPONENTS; i++)
	{
		EXPECT_EQ(cdo->GetCDOComponentsInfo().at(i).Size, COMPONENT_SIZE * (i + 1));
		EXPECT_EQ(cdo->GetCDOComponentsInfo().at(i).Alignment, COMPONENT_ALIGNMENT);
	}
};

class CWorldObjectCDOMock : public IWorldObjectCDO
{
public:
	MOCK_METHOD(const std::vector<CEntityComponentMetadata>&, GetCDOComponentsInfo, (), (const, override));
	MOCK_METHOD(uint64_t, GetClassSize, (), (const, override));
	MOCK_METHOD(uint64_t, GetClassAlignment, (), (const, override));
	MOCK_METHOD(uint64_t, ComputeComponentsMaxSizeForAllocation, (), (const, override));
};

TEST(CWorldObjectArchetypesComponentsContainerTest, MustNotDie)
{
	EXPECT_NO_FATAL_FAILURE(CWorldObjectArchetypesComponentsContainer container(nullptr, nullptr));
}

TEST(CWorldObjectArchetypesComponentsContainerTest, MustNotCallAnything)
{
	EXPECT_NO_FATAL_FAILURE(CWorldObjectArchetypesComponentsContainer container(nullptr, nullptr));
}

TEST(CWorldObjectArchetypesComponentsContainerTest, MustDieIfOnlyCDOIsProvided)
{
	const std::vector<CEntityComponentMetadata> componentsInfo{ CEntityComponentMetadata {4,4} };
	CWorldObjectCDOMock cdoMock{};
	EXPECT_CALL(cdoMock, GetClassSize).WillRepeatedly(::testing::Return(16));
	EXPECT_CALL(cdoMock, ComputeComponentsMaxSizeForAllocation).WillRepeatedly(::testing::Return(16));
	EXPECT_CALL(cdoMock, GetCDOComponentsInfo).WillRepeatedly(::testing::ReturnRef(componentsInfo));
	EXPECT_DEATH(CWorldObjectArchetypesComponentsContainer container(nullptr, &cdoMock), ".*");
}

TEST(CWorldObjectArchetypesComponentsContainerTest, MustNotDoAnythingIfHasNoComponents)
{
	const std::vector<CEntityComponentMetadata> emptyComponentsInfo;

	CWorldObjectCDOMock cdoMock{};
	EXPECT_CALL(cdoMock, GetClassSize).WillRepeatedly(::testing::Return(16));
	EXPECT_CALL(cdoMock, GetCDOComponentsInfo).WillRepeatedly(::testing::ReturnRef(emptyComponentsInfo));
	EXPECT_CALL(cdoMock, ComputeComponentsMaxSizeForAllocation).WillRepeatedly(::testing::Return(16));

	double a{};

	EXPECT_NO_FATAL_FAILURE(CWorldObjectArchetypesComponentsContainer container(reinterpret_cast<void*>(&a), &cdoMock));
}

TEST(CWorldObjectArchetypesComponentsContainerTest, MustDieIfClassSizeIsZero)
{
	const std::vector<CEntityComponentMetadata> componentsInfo{ CEntityComponentMetadata {4,4} };

	CWorldObjectCDOMock cdoMock{};
	EXPECT_CALL(cdoMock, GetCDOComponentsInfo).WillRepeatedly(::testing::ReturnRef(componentsInfo));
	EXPECT_CALL(cdoMock, GetClassSize).WillRepeatedly(::testing::Return(0));
	EXPECT_CALL(cdoMock, ComputeComponentsMaxSizeForAllocation).WillRepeatedly(::testing::Return(16));

	double a{};

	EXPECT_DEATH(CWorldObjectArchetypesComponentsContainer container(reinterpret_cast<void*>(&a), &cdoMock), ".*");
}

class CWorldObjectArchetypesComponentsContainerFixture : public ::testing::Test {
protected:
	CWorldObjectArchetypesComponentsContainer* ContainerUnderTest{};
	alignas(4) std::array<uint8_t, 64> Buffer;
	CWorldObjectCDOMock* cdoMock{};
	const std::vector<CEntityComponentMetadata> ComponentsInfo{ CEntityComponentMetadata{Buffer.size() / 2,4} };
	const std::vector<CEntityComponentMetadata> EmptyComponentsInfo{};
	void SetUp() override
	{
	}

	void InitializeWithNullptr()
	{
		ContainerUnderTest = new CWorldObjectArchetypesComponentsContainer(nullptr, nullptr);
	}

	void InitializeCorrectly(bool expectations = true)
	{
		cdoMock = new CWorldObjectCDOMock;
		if (expectations)
		{
			EXPECT_CALL(*cdoMock, GetCDOComponentsInfo).WillRepeatedly(::testing::ReturnRef(ComponentsInfo));
			EXPECT_CALL(*cdoMock, GetClassSize).WillRepeatedly(::testing::Return(Buffer.size() / 2));
			EXPECT_CALL(*cdoMock, ComputeComponentsMaxSizeForAllocation).WillRepeatedly(::testing::Return(Buffer.size() / 2));
		}

		ContainerUnderTest = new CWorldObjectArchetypesComponentsContainer(reinterpret_cast<void*>(Buffer.data()), cdoMock);
	}

	void TearDown() override {
		delete ContainerUnderTest;
		delete cdoMock;
	}

	void* MallocComponent(const uint64_t size, const uint64_t alignment) { return ContainerUnderTest->MallocComponent(size, alignment); }
	void FreeComponent(void* ptr) { ContainerUnderTest->FreeComponent(ptr); }
};

TEST_F(CWorldObjectArchetypesComponentsContainerFixture, MustNotDoAnythingWhenInitializedWithNullptrCDO)
{
	InitializeWithNullptr();
	EXPECT_NO_FATAL_FAILURE(MallocComponent(2, 2));
	EXPECT_NO_FATAL_FAILURE(MallocComponent(2, 2));
	EXPECT_NO_FATAL_FAILURE(MallocComponent(100, 8));
	EXPECT_NO_FATAL_FAILURE(FreeComponent(nullptr));
	EXPECT_NO_FATAL_FAILURE(FreeComponent((void*)0xfffffff));
}

TEST_F(CWorldObjectArchetypesComponentsContainerFixture, MustDieAllocatingZeroAlignment)
{
	InitializeCorrectly();
	EXPECT_DEATH(MallocComponent(4, 0), ".*");
}

TEST_F(CWorldObjectArchetypesComponentsContainerFixture, MustDieAllocatingNonPowerOfTwoAlignment)
{
	InitializeCorrectly();

	EXPECT_DEATH(MallocComponent(4, 3), ".*");
}

TEST_F(CWorldObjectArchetypesComponentsContainerFixture, MustDieAllocatingAlignmentLargerThanSize)
{
	InitializeCorrectly();

	EXPECT_DEATH(MallocComponent(4, 8), ".*");
}

TEST_F(CWorldObjectArchetypesComponentsContainerFixture, MustCorrectlyAllocateNComponentsAligned)
{
	InitializeCorrectly();

	void* a = MallocComponent(16, 4);
	void* b = MallocComponent(16, 4);
	void* c = MallocComponent(16, 4);

	EXPECT_EQ(reinterpret_cast<uintptr_t>(a) % 4, 0) << "Must be aligned pointer!";
	EXPECT_EQ(reinterpret_cast<uintptr_t>(b) % 4, 0) << "Must be aligned pointer!";
	EXPECT_EQ(c, nullptr);
}

TEST_F(CWorldObjectArchetypesComponentsContainerFixture, MustCorrectlyFillAllSpaceAndFreeIt)
{
	InitializeCorrectly();

	for (uint64_t i{}; i < 100; i++)
	{
		void* a = MallocComponent(16, 4);
		void* b = MallocComponent(16, 4);
		EXPECT_NE(a, nullptr);
		EXPECT_NE(b, nullptr);
		FreeComponent(b);
		FreeComponent(a);
	}
}

class CWorldObjectPendingDestroyNotifierMock : public IWorldObjectPendingDestroyNotifier
{
public:
	MOCK_METHOD(void, MarkPendingDestroy, (class CWorldObject*), (override));
};

TEST(CDestroyableTest, MustCallDestroyFn)
{
	CWorldObjectPendingDestroyNotifierMock mock;
	EXPECT_CALL(mock, MarkPendingDestroy).Times(1);
	CDestroyable destroyable(&mock);

	EXPECT_EQ(destroyable.IsPendingDestroy(), false);

	destroyable.SetPendingDestroy();

	EXPECT_EQ(destroyable.IsPendingDestroy(), true);
}

TEST(CDestroyableTest, MustCallDestroyCallback)
{
	CWorldObjectPendingDestroyNotifierMock mock;
	EXPECT_CALL(mock, MarkPendingDestroy).Times(2);
	CDestroyable destroyable(&mock);

	std::function<void()> callback = [&mock]() { mock.MarkPendingDestroy(nullptr); };
	destroyable.OnSetPendingDestroyCallback(callback);

	destroyable.SetPendingDestroy();
}

TEST(CWorldObjectTest, MustConstructCorrectly)
{
	DWorldObjectInitializer initializer{};
	initializer.ClassSize = sizeof(CWorldObject);
	initializer.ClassAlignment = alignof(CWorldObject);

	EXPECT_NO_FATAL_FAILURE(CWorldObject object(initializer, true));
}

TEST(CWorldObjectTest, MustRegisterCDOComponentsCorrectly)
{
	struct ComponentFoo
	{
		char Foo[4];
	};

	struct ComponentBar
	{
		uint32_t Bar[4];
	};

	const std::vector<CEntityComponentMetadata> ExpectedComponentsInfo{ CEntityComponentMetadata {sizeof(ComponentFoo), alignof(ComponentFoo)},CEntityComponentMetadata{sizeof(ComponentBar), alignof(ComponentBar)} };


	class CTestObject : public CWorldObject
	{
	public:
		CTestObject(const DWorldObjectInitializer& initializer) :CWorldObject(initializer, true) {
			NewComponent<ComponentFoo>();
			NewComponent<ComponentBar>();
		}
		bool A{};
	};


	DWorldObjectInitializer initializer{};
	initializer.ClassSize = sizeof(CTestObject);
	initializer.ClassAlignment = alignof(CTestObject);

	CTestObject object(initializer);

	EXPECT_EQ(object.GetCDOComponentsInfo().size(), ExpectedComponentsInfo.size());
	EXPECT_EQ(object.GetCDOComponentsInfo().at(0).Size, ExpectedComponentsInfo.at(0).Size);
	EXPECT_EQ(object.GetCDOComponentsInfo().at(0).Alignment, ExpectedComponentsInfo.at(0).Alignment);
	EXPECT_EQ(object.GetCDOComponentsInfo().at(1).Size, ExpectedComponentsInfo.at(1).Size);
	EXPECT_EQ(object.GetCDOComponentsInfo().at(1).Alignment, ExpectedComponentsInfo.at(1).Alignment);
}

#pragma endregion


#pragma endregion

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}