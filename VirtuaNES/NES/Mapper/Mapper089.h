//////////////////////////////////////////////////////////////////////////
// Mapper089  SunSoft (���ˉ���)                                        //
//////////////////////////////////////////////////////////////////////////
class	Mapper089 : public Mapper
{
public:
	Mapper089( NES* parent ) : Mapper(parent) {}

	void	Reset();
	void	Write(WORD addr, BYTE data);

protected:
private:
};
