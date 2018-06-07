//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop //for control over precompiled header file to use or create
#include <tchar.h>

//Klasa TForm1 - g³ówne okno
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
class TForm1 : public TForm
{
	__published:	// IDE-managed Components
	private:	// User declarations
	public:		// User declarations
	__fastcall TForm1(TComponent* Owner): TForm(Owner){}; //kostruktor klasy
	void test()
	{
		ShowMessage("In test1");
	}
};
//---------------------------------------------------------------------------
#pragma resource "*.dfm" //wymagany plik z w³aœciwoœciami dla formularza

TForm1 *Form1; //Przypisanie obiektu do zmiennej "Form1"
WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR, int)
{
	try
	{
		Application->Initialize();
		Application->MainFormOnTaskBar = true;
		Application->CreateForm(__classid(TForm1), &Form1); //<---
		Application->Run();
	}
	catch (Exception &exception)
	{
		Application->ShowException(&exception);
	}
	catch (...)
	{
		try
		{
			throw Exception("");
		}
		catch (Exception &exception)
		{
			Application->ShowException(&exception);
		}
	}
	return 0;
}
//---------------------------------------------------------------------------
